// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dispatch_model.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <format>
#include <map>
#include <random>
#include <utility>

#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>

#include "accepted_breakdown.h"
#include "dates.h"
#include "flight_tracker.h"
#include "simbrief.h"

using namespace blocktime;

namespace {

std::string utcNowInstant() {
    return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

QString q(const std::string& s) { return QString::fromStdString(s); }
QString q(std::string_view s) { return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())); }
QString pounds(int lb) { return q(formatNumber(lb)) + " lb"; }

QString hoursMinutes(int seconds) {
    const int minutes = (seconds + 30) / 60;
    return QString::number(minutes / 60) + ":" + QString::number(minutes % 60).rightJustified(2, '0');
}

QString holdShort(const std::string& hold) {
    if (hold == "main_deck") return "main";
    if (hold == "forward") return "fwd";
    return q(hold);
}

QString cabinLine(const CabinLoad& c, FlightKind kind) {
    const QString n = q(formatNumber(c.passengers));
    if (kind == FlightKind::Vip) return n + " VIP passengers";
    if (kind == FlightKind::Charter) return n + " charter passengers";
    return n + " " + q(c.cabinClass);
}

QString holdName(const std::string& hold) {
    if (hold == "main_deck") return "Main deck";
    QString name = q(hold);
    name[0] = name[0].toUpper();
    return name;
}

QString toneName(BoardRow::Tone tone) {
    switch (tone) {
        case BoardRow::Tone::Income: return "income";
        case BoardRow::Tone::Cost: return "cost";
        case BoardRow::Tone::Profit: return "profit";
        case BoardRow::Tone::Loss: return "loss";
    }
    return "cost";
}

// A fresh draft's day band and roll (D-104, D-089): day or night by the origin's local time now.
std::pair<bool, Roll> dayAndRoll(const FlightAirplane& airplane) {
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const int hour = localHour(airplane.originTimeZone, now)
                         .value_or(static_cast<int>(
                             std::chrono::hh_mm_ss{now - std::chrono::floor<std::chrono::days>(now)}.hours().count()));
    auto engine = std::make_shared<std::mt19937_64>(std::random_device{}());
    Roll roll = [engine] { return std::uniform_real_distribution<double>(0.0, 1.0)(*engine); };
    return {isDaytime(hour), std::move(roll)};
}

const MoneyLine* findLine(const Preview& p, const std::string& id) {
    for (const auto* lines : {&p.income, &p.costs}) {
        for (const MoneyLine& m : *lines) {
            if (m.category == id) return &m;
        }
    }
    return nullptr;
}

Cents incomeOf(const Preview& p) {
    Cents sum = 0;
    for (const MoneyLine& m : p.income) sum += m.cents;
    return sum;
}

Cents costsOf(const Preview& p) {
    Cents sum = 0;
    for (const MoneyLine& m : p.costs) sum -= m.cents;
    return sum;
}

}  // namespace

DispatchModel::DispatchModel(db::Database& db, QString airlineIcao, std::filesystem::path ratesFile, Planner& planner,
                             RecorderBridge& bridge, AppEventQueue& appEvents, YourFlightModel& yourFlight,
                             QObject* parent)
    : QObject(parent), db_(db), airlineIcao_(std::move(airlineIcao)), ratesFile_(std::move(ratesFile)),
      planner_(planner), bridge_(bridge), appEvents_(appEvents), yourFlight_(yourFlight) {
    reloadRates();
    connect(&yourFlight_, &FlightSource::changed, this, &DispatchModel::changed);
    connect(&bridge_, &RecorderBridge::blockEvent, this, [this](const QString& type) { onBlockEvent(type); });
}

void DispatchModel::reloadRates() {
    std::string error;
    const auto rates = loadRates(ratesFile_, &error);
    ratesOk_ = rates.has_value();
    rates_ = rates.value_or(Rates{});
    if (!ratesOk_) message_ = q(error);
}

void DispatchModel::setMessage(QString message) {
    message_ = std::move(message);
    emit changed();
}

// --- the form ---

void DispatchModel::openForm(FlightDraft draft, bool changing) {
    draft_.emplace(std::move(draft));
    changing_ = changing;
    ++request_;
    busy_ = false;
    message_.clear();
    yourFlight_.setFormOpen(true);
    emit changed();
}

bool DispatchModel::openForAirplane(const QString& registration) {
    if (view()) {
        setMessage("Flight " + q(view()->flightNumber) + " is already planned: cancel it before planning another.");
        return false;
    }
    const auto airplane = loadFlightAirplane(db_, registration.toStdString());
    if (!airplane) return false;
    reloadRates();
    auto [daytime, roll] = dayAndRoll(*airplane);
    destination_.clear();
    additionalFuel_.clear();
    openForm(FlightDraft(*airplane, rates_, daytime, std::move(roll)), false);
    if (!ratesOk_) reloadRates();  // openForm cleared the message: show why Dispatch is off
    return true;
}

void DispatchModel::openEmpty() {
    draft_.reset();
    changing_ = false;
    ++request_;
    yourFlight_.setFormOpen(false);
    emit changed();
}

void DispatchModel::closeForm() { openEmpty(); }

void DispatchModel::changeFlight() {
    const auto& v = view();
    if (!v || accepted() || locked()) return;  // Change flight is offered before Accept only (D-251)
    const auto airplane = loadFlightAirplane(db_, v->registration);
    if (!airplane) return;
    reloadRates();
    auto [daytime, roll] = dayAndRoll(*airplane);
    FlightDraft draft = FlightDraft::fromPlanned(*airplane, rates_, *v, daytime, std::move(roll));
    draft.setDestination(checkDestination(db_, *airplane, v->destination));
    destination_ = q(v->destination);
    additionalFuel_ = v->additionalFuelLb > 0 ? QString::number(v->additionalFuelLb) : QString();
    openForm(std::move(draft), true);
}

void DispatchModel::dispatch() {
    if (!canDispatch()) return;
    busy_ = true;
    message_.clear();
    emit changed();
    // The flight as it is now, planned at the click (D-105); the runways are filled in when they arrive.
    const NewPlannedFlight clicked = draft_->toNew(utcNowInstant(), {});
    const bool changing = changing_;
    const std::int64_t id = changing && view() ? view()->id : 0;
    const int request = ++request_;
    planner_.fetchRunways(q(clicked.originIcao), q(clicked.destinationIcao),
                          [this, clicked, changing, id, request](AtisRunways runways, QString note) {
        if (request != request_) return;  // cancelled, or the form reopened since
        busy_ = false;
        NewPlannedFlight flight = clicked;
        flight.originRunway = runways.origin;
        flight.destinationRunway = runways.destination;
        std::string error;
        const bool ok = changing ? updatePlannedFlight(db_, id, flight, &error)
                                 : createPlannedFlight(db_, flight, &error).has_value();
        if (!ok) {
            setMessage(q(error));
            return;
        }
        draft_.reset();
        changing_ = false;
        before_.reset();
        changedIds_.clear();
        selectedId_.clear();
        yourFlight_.setFormOpen(false);
        yourFlight_.reload(note);
        message_ = note;
        emit changed();
        emit dispatched();
    });
}

QString DispatchModel::registration() const { return draft_ ? q(draft_->airplane().registration) : QString(); }

QString DispatchModel::airplaneText() const {
    if (!draft_) return {};
    return q(draft_->airplane().registration + " · " + draft_->airplane().typeCode);
}

QString DispatchModel::origin() const {
    if (!draft_) return {};
    return q(draft_->airplane().originIcao + " · " + draft_->airplane().originName);
}

QStringList DispatchModel::kinds() const {
    QStringList out;
    if (draft_) {
        for (FlightKind k : draft_->kinds()) out << q(kindCode(k));
    }
    return out;
}

QStringList DispatchModel::kindLabels() const {
    QStringList out;
    if (draft_) {
        for (FlightKind k : draft_->kinds()) out << q(kindLabel(k));
    }
    return out;
}

QString DispatchModel::kind() const { return draft_ ? q(kindCode(draft_->kind())) : QString(); }

void DispatchModel::setKind(const QString& code) {
    const auto kind = parseKind(code.toStdString());
    if (!draft_ || !kind) return;
    if (std::find(draft_->kinds().begin(), draft_->kinds().end(), *kind) == draft_->kinds().end()) return;
    draft_->setKind(*kind);
    emit changed();
}

void DispatchModel::setFullyLoaded(bool on) {
    if (!draft_) return;
    draft_->setFullyLoaded(on);
    emit changed();
}

void DispatchModel::setBellyCargo(bool on) {
    if (!draft_) return;
    draft_->setBellyCargo(on);
    emit changed();
}

void DispatchModel::setDestination(const QString& typed) {
    if (!draft_) return;
    destination_ = typed;
    draft_->setDestination(checkDestination(db_, draft_->airplane(), typed.toStdString()));
    emit changed();
}

QString DispatchModel::destinationMessage() const {
    if (!draft_ || destination_.trimmed().isEmpty()) return {};
    return q(draft_->destination().message);
}

QString DispatchModel::flightNumber() const { return draft_ ? q(draft_->flightNumber()) : QString(); }

void DispatchModel::setFlightNumber(const QString& number) {
    if (!draft_) return;
    draft_->setFlightNumber(number.trimmed().toStdString());
    emit changed();
}

void DispatchModel::setAdditionalFuel(const QString& text) {
    QString digits;
    for (const QChar ch : text) {
        if (ch.isDigit() && digits.size() < 6) digits += ch;
    }
    additionalFuel_ = digits;
    if (draft_) draft_->setAdditionalFuelLb(digits.isEmpty() ? 0 : digits.toInt());
    emit changed();
}

QStringList DispatchModel::loadLines() const {
    if (!draft_) return {};
    if (draft_->kind() == FlightKind::Delivery) return {"Delivery flight: no passengers or cargo"};
    const LoadRoll l = draft_->loads();
    QStringList out;
    for (const CabinLoad& c : l.cabins) out << cabinLine(c, draft_->kind());
    const int bags = bagsLb(l.cabins, draft_->airplane().limits);
    if (bags > 0) out << "Bags " + pounds(bags);
    if (l.cargoLb > 0) out << "Cargo " + pounds(l.cargoLb);
    QStringList holds;
    for (const HoldLoad& h : draft_->holds()) holds << holdName(h.hold) + " " + pounds(h.weightLb);
    if (bags + l.cargoLb > 0) out << holds.join(" · ");
    return out;
}

bool DispatchModel::canDispatch() const { return draft_ && ratesOk_ && draft_->canPlan() && !busy_; }

// --- the report's actions ---

ChangeFacts DispatchModel::factsOf(const PlannedFlightView& v) const {
    ChangeFacts f;
    if (v.plan) {
        f.blockS = v.plan->estBlockS;
        f.fuelAddedLb = std::max(0, v.plan->rampFuelLb - v.fuelOnRecordLb);
        f.extraFuelLb = v.plan->extraFuelLb;
    }
    return f;
}

std::optional<Preview> DispatchModel::preview() const {
    const auto& v = view();
    if (!v) return std::nullopt;
    if (v->acceptedAt) {  // what Dave accepted, with its locked rates (D-222), whatever rates.yaml says now
        if (const auto accepted = plannedAcceptance(db_, v->id)) return accepted->money;
    }
    return pricePreview(priceInput(*v), rates_);
}

void DispatchModel::openSimbrief() {
    const auto& v = view();
    if (!v) return;
    DispatchRequest r;
    r.origin = v->origin;
    r.destination = v->destination;
    r.airlineIcao = airlineIcao_.toStdString();
    r.flightNumber = v->flightNumber;
    r.airframeId = v->airframeId;
    r.originRunway = v->originRunway;
    r.destinationRunway = v->destinationRunway;
    for (const CabinLoad& c : v->cabins) r.passengers += c.passengers;
    r.cargoLb = v->cargoLb;
    r.additionalFuelLb = v->additionalFuelLb;
    planner_.openUrl(q(dispatchUrl(r)));
}

void DispatchModel::importPlan() {
    if (!view() || busy_ || locked()) return;  // after block out the plan can't change anything (review 4.6)
    busy_ = true;
    message_.clear();
    emit changed();
    const int request = ++request_;
    planner_.fetchLatestOfp([this, request](QByteArray body, QString networkError) {
        if (request != request_) return;
        busy_ = false;
        const auto& v = view();
        if (!v) {
            emit changed();
            return;
        }
        if (!networkError.isEmpty()) return setMessage(networkError);
        std::string error;
        const auto plan = parseOfp(body.toStdString(), &error);
        if (!plan) return setMessage("SimBrief: " + q(error));
        const PlanKey key{v->origin, v->destination, v->airframeId, v->registration, v->plannedAt};
        if (const auto mismatch = planMismatch(*plan, key)) return setMessage(q(*mismatch));
        int passengers = 0;
        for (const CabinLoad& c : v->cabins) passengers += c.passengers;
        if (const auto mismatch = loadMismatch(*plan, {passengers, v->cargoLb, v->flightNumber})) {
            return setMessage(q(*mismatch));  // the plan, the breakdown and its rates stay as they were
        }

        // What the report showed before, for the rows that changed (D-251: in memory only).
        std::optional<Preview> before;
        ChangeFacts beforeFacts;
        if (v->plan) {
            before = preview();
            beforeFacts = factsOf(*v);
        }
        const auto accepted = plannedAcceptance(db_, v->id);
        const std::int64_t id = v->id;
        {
            db::Transaction tx(db_);
            if (!tx.active()) return setMessage("Couldn't store the plan: " + q(tx.error()));
            if (!storeSimbriefPlan(db_, id, *plan, body.toStdString(), utcNowInstant(), &error)) {
                return setMessage("Couldn't store the plan: " + q(error));
            }
            if (accepted) {
                // After Accept a re-import replaces the breakdown without asking, at the rates locked then (D-233):
                // the plan's block time for crew and maintenance, its fuel to add for fuel.
                const Preview again =
                    settleAccepted(accepted->money, plan->estBlockS / 3600.0,
                                   std::max(0, plan->rampFuelLb - v->fuelOnRecordLb), v->fuelPriceCentsPer100Lb);
                if (!writeAcceptance(db_, id, again, utcNowInstant(), false, &error)) {
                    return setMessage("Couldn't store the plan: " + q(error));
                }
            }
            if (!tx.commit(&error)) return setMessage("Couldn't store the plan: " + q(error));
        }
        yourFlight_.reload();
        changedIds_.clear();
        before_.reset();
        if (!before) {  // the first import flips every row in
            for (const BoardRow& r : blocktime::boardRows(*preview())) changedIds_.insert(r.id);
        } else if (view()) {
            for (const LineChange& c : changesSince(*before, beforeFacts, *preview(), factsOf(*view()))) {
                changedIds_.insert(c.id);
            }
            before_ = std::move(before);
            beforeFacts_ = beforeFacts;
            beforeFlightId_ = id;
        }
        ++importCount_;
        message_.clear();
        emit changed();
    });
}

void DispatchModel::accept() {
    const auto& v = view();
    if (!v || !imported() || accepted() || locked()) return;
    const auto money = preview();
    std::string error;
    if (!money || !acceptPlannedFlight(db_, v->id, *money, utcNowInstant(), false, &error)) {
        return setMessage("Couldn't accept the flight: " + q(error));
    }
    // The flight's start in the log (D-236); the recorder adds the cockpit and telemetry snapshots.
    if (const auto target = flightTarget(db_, *v)) appEvents_.push("flight_accepted", acceptedEventData(*target, false));
    changedIds_.clear();
    before_.reset();
    selectedId_.clear();
    message_.clear();
    yourFlight_.reload();  // the recorder's target now says accepted
    emit changed();
}

void DispatchModel::cancelFlight() {
    yourFlight_.cancelFlight();  // keeps its guards: not while flying, nor flown and waiting to settle
    if (view()) return setMessage(yourFlight_.message());
    draft_.reset();
    changing_ = false;
    before_.reset();
    changedIds_.clear();
    selectedId_.clear();
    message_.clear();
    yourFlight_.setFormOpen(false);
    emit changed();
}

void DispatchModel::onBlockEvent(const QString& type) {
    if (type != "block_out") {
        emit changed();
        return;
    }
    // Released with a plan but not accepted: accepted now, with this moment's breakdown and rates (D-250). The
    // recorder wrote the marker before block_out.
    const auto& v = view();
    if (v && v->plan && !v->acceptedAt) {
        std::string error;
        if (const auto money = preview()) {
            if (!acceptPlannedFlight(db_, v->id, *money, utcNowInstant(), true, &error)) {
                message_ = "Couldn't accept the flight at block out: " + q(error);
            }
        }
        yourFlight_.reload();
    }
    emit changed();
}

void DispatchModel::select(const QString& id) {
    selectedId_ = (id == selectedId_) ? QString() : id;
    emit changed();
}

void DispatchModel::copyRoute() {
    const auto& v = view();
    if (v && v->plan) QGuiApplication::clipboard()->setText(q(routeWithRunways(*v->plan)));
}

// --- the report ---

QString DispatchModel::reportTitle() const {
    const auto& v = view();
    if (!v) return {};
    return airlineIcao_ + " " + q(v->flightNumber) + QString::fromUtf8(" · ") + q(v->origin) +
           QString::fromUtf8(" → ") + q(v->destination);
}

QString DispatchModel::extraFuelBadge() const {
    const auto& v = view();
    if (!v) return {};
    const int lb = v->plan ? v->plan->extraFuelLb : v->additionalFuelLb;  // the OFP's EXTRA once it's in
    return lb > 0 ? "+" + pounds(lb) + " extra fuel" : QString();
}

QStringList DispatchModel::warnings() const {
    const auto& v = view();
    QStringList out;
    if (!v || !v->plan) return out;
    if (nearMlw(*v)) out << "Landing weight near MLW";
    if (v->plan->rampFuelLb < v->fuelOnRecordLb) out << "More fuel on board than the plan's block fuel";
    return out;
}

bool DispatchModel::imported() const { return view() && view()->plan.has_value(); }
bool DispatchModel::accepted() const { return view() && view()->acceptedAt.has_value(); }

bool DispatchModel::locked() const {
    const YourFlightStage stage = yourFlight_.stage();
    return bridge_.flightOpen() || stage == YourFlightStage::Flying || stage == YourFlightStage::Arrived;
}

QVariantList DispatchModel::boardRows() const {
    const auto p = preview();
    if (!p) return {};
    std::vector<BoardRow> rows = blocktime::boardRows(*p);
    const bool shown = imported();
    if (!shown) {
        // Before import: the labels only, with a fuel row waiting for the plan.
        const auto firstCost = std::find_if(rows.begin(), rows.end(),
                                            [](const BoardRow& r) { return r.tone == BoardRow::Tone::Cost; });
        rows.insert(firstCost, BoardRow{"fuel", "Fuel", "", BoardRow::Tone::Cost});
    }
    QVariantList out;
    for (BoardRow& r : rows) {
        if (!shown) r.value.clear();
        out << QVariantMap{{"id", q(r.id)},
                           {"text", q(boardText(r))},
                           {"valueStart", 20 - static_cast<int>(r.value.size())},
                           {"tone", toneName(r.tone)},
                           {"changed", changedIds_.contains(r.id)}};
    }
    return out;
}

QVariantMap DispatchModel::ring() const {
    QVariantList slices;
    bool loss = false;
    const auto p = imported() ? preview() : std::nullopt;
    if (p) {
        const Ring r = blocktime::ring(*p);
        loss = r.loss;
        std::map<std::string, int> shades;  // the slice's place in its colour family
        for (const RingSlice& s : r.slices) {
            const MoneyLine* line = findLine(*p, s.id);
            const std::string label = s.id == "profit" ? "Profit" : boardLabel(s.id, line ? line->label : s.id);
            slices << QVariantMap{{"id", q(s.id)},
                                  {"family", q(s.family)},
                                  {"label", q(label)},
                                  {"amountText", q(s.id == "profit" ? formatMoneyFull(s.cents)
                                                                    : formatMoneyFull(-s.cents))},
                                  {"shade", shades[s.family]++},
                                  {"start", s.start},
                                  {"sweep", s.sweep}};
        }
    }
    return {{"loss", loss}, {"slices", slices}};
}

QString DispatchModel::centerLabel() const {
    const auto p = imported() ? preview() : std::nullopt;
    if (!p) return {};
    if (const MoneyLine* m = findLine(*p, selectedId_.toStdString())) return q(boardLabel(m->category, m->label));
    return p->totalCents < 0 ? QStringLiteral("Loss") : QStringLiteral("Profit");
}

QString DispatchModel::centerValue() const {
    const auto p = imported() ? preview() : std::nullopt;
    if (!p) return QString::fromUtf8("—");
    if (const MoneyLine* m = findLine(*p, selectedId_.toStdString())) {
        return q(boardValue(m->cents, m->cents > 0 ? BoardRow::Tone::Income : BoardRow::Tone::Cost));
    }
    return q(formatMoneyFull(p->totalCents));
}

QString DispatchModel::centerSub() const {
    const auto p = imported() ? preview() : std::nullopt;
    if (!p) return {};
    const bool loss = blocktime::ring(*p).loss;
    if (const MoneyLine* m = findLine(*p, selectedId_.toStdString())) {
        if (m->cents > 0) return "the whole circle";
        const Cents circle = loss ? costsOf(*p) : incomeOf(*p);
        if (circle <= 0) return {};
        const long long percent = std::llround(100.0 * static_cast<double>(-m->cents) / static_cast<double>(circle));
        return QString::number(percent) + (loss ? "% of the costs" : "% of the fares");
    }
    if (loss) return "on " + q(formatMoneyFull(costsOf(*p))) + " costs";
    return "of " + q(formatMoneyFull(incomeOf(*p))) + " fares";
}

QString DispatchModel::centerTone() const {
    const auto p = imported() ? preview() : std::nullopt;
    if (!p) return "none";
    if (const MoneyLine* m = findLine(*p, selectedId_.toStdString())) return m->cents > 0 ? "income" : "cost";
    return p->totalCents < 0 ? "loss" : "profit";
}

QVariantMap DispatchModel::selection() const {
    const auto p = imported() ? preview() : std::nullopt;
    const auto& v = view();
    if (!p || !v || selectedId_.isEmpty()) return {};
    const std::string id = selectedId_.toStdString();
    QVariantMap out;
    QStringList lines;
    if (id == "profit") {
        const BoardRow::Tone tone = p->totalCents < 0 ? BoardRow::Tone::Loss : BoardRow::Tone::Profit;
        out["label"] = tone == BoardRow::Tone::Loss ? "Loss" : "Profit";
        out["amountText"] = q(boardValue(p->totalCents, tone));
        out["tone"] = toneName(tone);
        lines << "Fares " + q(formatMoneyFull(incomeOf(*p))) << QString::fromUtf8("− costs ") + q(formatMoneyFull(costsOf(*p)));
    } else {
        const MoneyLine* m = findLine(*p, id);
        if (!m) return {};
        const BoardRow::Tone tone = m->cents > 0 ? BoardRow::Tone::Income : BoardRow::Tone::Cost;
        out["label"] = q(boardLabel(m->category, m->label));
        out["amountText"] = q(boardValue(m->cents, tone));
        out["tone"] = toneName(tone);
        WorkingFacts facts;
        facts.rampFuelLb = v->plan ? v->plan->rampFuelLb : 0;
        facts.fuelOnRecordLb = v->fuelOnRecordLb;
        facts.extraFuelLb = v->plan ? v->plan->extraFuelLb : 0;
        facts.locked = locked();
        for (const std::string& line : workingLines(*m, facts)) lines << q(line);
    }
    out["lines"] = lines;
    QString note;
    if (before_ && beforeFlightId_ == v->id) {
        for (const LineChange& c : changesSince(*before_, beforeFacts_, *p, factsOf(*v))) {
            if (c.id == id) note = q(c.why);
        }
    }
    out["note"] = note;
    return out;
}

QString DispatchModel::hint() const {
    if (!imported()) return "Import the SimBrief plan to see what this flight earns and costs. Accept unlocks then.";
    return "Click a line on the board, or a slice of the ring, to see how it was worked out.";
}

QString DispatchModel::loadTitle() const {
    const auto& v = view();
    if (!v) return {};
    return QString::fromUtf8("LOAD · ") + q(v->registration) + " " + q(v->typeCode) + QString::fromUtf8(" · ") +
           q(kindLabel(v->kind)).toUpper();
}

int DispatchModel::seatsFilled() const {
    int n = 0;
    if (view()) {
        for (const CabinLoad& c : view()->cabins) n += c.passengers;
    }
    return n;
}

int DispatchModel::seatsTotal() const { return view() ? view()->seatsInstalled : 0; }

int DispatchModel::seatSegmentsFilled() const {
    const int total = seatsTotal();
    return total > 0 ? static_cast<int>(std::llround(static_cast<double>(seatsFilled()) * seatSegments() / total)) : 0;
}

QString DispatchModel::loadText() const {
    const auto& v = view();
    if (!v) return {};
    if (v->kind == FlightKind::Delivery) return "Delivery flight: no passengers or cargo";
    QStringList parts;
    if (v->seatsInstalled > 0) parts << QString::number(seatsFilled()) + " of " + QString::number(v->seatsInstalled) + " seats";
    const int bags = bagsLb(v->cabins, v->limits);
    if (bags > 0) parts << pounds(bags) + " bags";
    if (v->cargoLb > 0) parts << pounds(v->cargoLb) + " cargo";
    int holdTotal = 0;
    for (const Hold& h : v->holds) holdTotal += h.maxLb;
    if (bags + v->cargoLb > 0 && bags + v->cargoLb <= holdTotal) {
        QStringList holds;
        for (const HoldLoad& h : splitHolds(v->holds, bags + v->cargoLb)) {
            holds << q(formatNumber(h.weightLb)) + " " + holdShort(h.hold);
        }
        parts << "holds " + holds.join(" / ");
    }
    return parts.join(QString::fromUtf8(" · "));
}

QVariantList DispatchModel::weightBars() const {
    const auto& v = view();
    if (!v) return {};
    QVariantList out;
    for (const WeightBar& b : blocktime::weightBars(*v)) {
        const double top = 1.2 * std::max(b.valueLb.value_or(0), b.limitLb.value_or(0));
        out << QVariantMap{
            {"label", q(b.label)},
            {"valueText", b.valueLb ? q(formatNumber(*b.valueLb)) : QStringLiteral("after import")},
            {"limitText", b.limitLb ? "/ " + q(formatNumber(*b.limitLb)) : QString()},
            {"share", b.valueLb && top > 0 ? *b.valueLb / top : 0.0},
            {"limitShare", b.limitLb && top > 0 ? *b.limitLb / top : 0.0}};
    }
    return out;
}

QString DispatchModel::planTitle() const {
    const auto& v = view();
    if (!v || !v->plan) return QString::fromUtf8("PLAN · WAITING FOR SIMBRIEF");
    const std::string& at = v->plan->generatedAt;
    return QString::fromUtf8("PLAN · GENERATED ") + (at.size() >= 16 ? q(at.substr(11, 5)) + "Z" : q(at));
}

QString DispatchModel::originIcao() const { return view() ? q(view()->origin) : QString(); }
QString DispatchModel::destinationIcao() const { return view() ? q(view()->destination) : QString(); }

QString DispatchModel::departureText() const {
    const auto& v = view();
    if (!v || !v->plan) return {};
    QStringList parts;
    if (!v->plan->originRunway.empty()) parts << "Runway " + q(v->plan->originRunway);
    if (v->brief && !v->brief->sid.empty()) parts << q(v->brief->sid);
    return parts.join(QString::fromUtf8(" · "));
}

QString DispatchModel::arrivalText() const {
    const auto& v = view();
    if (!v || !v->plan) return {};
    QStringList parts;
    if (v->brief && !v->brief->star.empty()) parts << q(v->brief->star);
    if (!v->plan->destinationRunway.empty()) parts << "runway " + q(v->plan->destinationRunway);
    return parts.join(QString::fromUtf8(" · "));
}

QVariantList DispatchModel::planCells() const {
    const auto& v = view();
    if (!v || !v->plan) return {};
    const SimbriefPlan& p = *v->plan;
    // Arrival if it left now, in Dave's local time.
    const QString arrive = QDateTime::currentDateTime().addSecs(p.estBlockS).toString("h:mm AP");
    return {QVariantMap{{"label", "CRUISE"}, {"value", "FL" + QString::number(p.cruiseAltitudeFt / 100)}},
            QVariantMap{{"label", "DISTANCE"}, {"value", q(formatNumber(std::llround(v->distanceNm))) + " nm"}},
            QVariantMap{{"label", "BLOCK"}, {"value", hoursMinutes(p.estBlockS)}},
            QVariantMap{{"label", "BLOCK FUEL"}, {"value", pounds(p.rampFuelLb)}},
            QVariantMap{{"label", "BURN"}, {"value", pounds(p.enrouteBurnLb)}},
            QVariantMap{{"label", "ARRIVE"}, {"value", arrive}}};
}

QString DispatchModel::routeForCharts() const {
    const auto& v = view();
    return v && v->plan ? q(routeWithRunways(*v->plan)) : QString();
}

QString DispatchModel::openSimbriefLabel() const { return imported() ? "Open SimBrief again" : "Open SimBrief"; }
QString DispatchModel::importLabel() const { return imported() ? "Re-import from SimBrief" : "Import from SimBrief"; }
