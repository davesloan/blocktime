// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "add_aircraft_model.h"

#include <algorithm>
#include <format>

#include <QBuffer>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QVariantMap>

#include "airframe_read.h"
#include "airline_queries.h"
#include "money.h"
#include "money_lines_qml.h"

using namespace blocktime;

namespace {

struct TextField {
    const char* key;
    const char* label;
    std::string SimbriefReading::*member;
    bool required;
};
struct NumberField {
    const char* key;
    const char* label;
    std::optional<int> SimbriefReading::*member;
    const char* unit;
};

constexpr TextField kTextFields[] = {
    {"registration", "Registration", &SimbriefReading::registration, true},
    {"finNumber", "Tail (Fin Number)", &SimbriefReading::finNumber, false},
    {"internalId", "Internal ID", &SimbriefReading::internalId, true},
    {"baseType", "Base type", &SimbriefReading::baseType, true},
};
constexpr NumberField kNumberFields[] = {
    {"maxPassengers", "Max passengers", &SimbriefReading::maxPassengers, ""},
    {"passengerWeightLb", "Passenger weight", &SimbriefReading::passengerWeightLb, "lb"},
    {"bagWeightLb", "Bag weight", &SimbriefReading::bagWeightLb, "lb"},
    {"emptyWeightLb", "Empty weight", &SimbriefReading::emptyWeightLb, "lb"},
    {"mzfwLb", "MZFW", &SimbriefReading::mzfwLb, "lb"},
    {"mtowLb", "MTOW", &SimbriefReading::mtowLb, "lb"},
    {"mlwLb", "MLW", &SimbriefReading::mlwLb, "lb"},
    {"maxFuelLb", "Max fuel", &SimbriefReading::maxFuelLb, "lb"},
    {"maxCargoLb", "Max cargo", &SimbriefReading::maxCargoLb, "lb"},
    {"minRunwayFt", "Min runway", &SimbriefReading::minRunwayFt, "ft"},
};

QString q(const std::string& s) { return QString::fromStdString(s); }

QString holdName(const std::string& hold) {
    if (hold == "main_deck") return QStringLiteral("Main deck");
    QString name = q(hold);
    name[0] = name[0].toUpper();
    return name;
}

QString seatsText(const CatalogAirframe& layout) {
    if (layout.cabins.empty()) return QStringLiteral("Freighter, no seats");
    int total = 0;
    QStringList parts;
    for (const Cabin& c : layout.cabins) {
        total += c.seats;
        parts.push_back(QStringLiteral("%1 %2").arg(c.seats).arg(q(c.cabinClass)));
    }
    if (parts.size() == 1) return QStringLiteral("%1 seats").arg(total);
    return parts.join(QStringLiteral(" + ")) + QStringLiteral(" = %1 seats").arg(total);
}

}  // namespace

AddAircraftModel::AddAircraftModel(db::Database& db, const Catalog& catalog, std::filesystem::path ratesFile,
                                   ReviewClient& claude, std::function<Date()> today, QObject* parent)
    : QObject(parent),
      db_(db),
      catalog_(catalog),
      rates_(loadRates(ratesFile, nullptr).value_or(Rates{})),
      claude_(claude),
      today_(std::move(today)) {}

void AddAircraftModel::clear() {
    ++attempt_;  // an answer still on its way is dropped
    state_ = QStringLiteral("empty");
    readError_.clear();
    image_.clear();
    reading_ = {};
    layouts_.clear();
    layoutIndex_ = -1;
    flightNumber_.clear();
    delivery_ = QStringLiteral("factory");
    deliveryIcao_.clear();
    leaseError_.clear();
    emit changed();
}

void AddAircraftModel::readImage(const QByteArray& bytes, const QString& mediaType) {
    image_ = bytes;
    mediaType_ = mediaType;
    send();
}

void AddAircraftModel::readImageFile(const QUrl& file) {
    QFile f(file.toLocalFile());
    if (!f.open(QIODevice::ReadOnly)) {
        readError_ = QStringLiteral("Couldn't open %1.").arg(file.toLocalFile());
        emit changed();
        return;
    }
    const QString suffix = QFileInfo(f).suffix().toLower();
    readImage(f.readAll(), suffix == "png" ? QStringLiteral("image/png") : QStringLiteral("image/jpeg"));
}

void AddAircraftModel::readClipboard() {
    auto* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    const QImage image = app ? QGuiApplication::clipboard()->image() : QImage();
    if (image.isNull()) {
        readError_ = QStringLiteral("The clipboard has no picture. Copy the screenshot first (Win+Shift+S).");
        emit changed();
        return;
    }
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    readImage(bytes, QStringLiteral("image/png"));
}

void AddAircraftModel::retry() {
    if (!image_.isEmpty()) send();
}

void AddAircraftModel::send() {
    leaseError_.clear();
    if (!claude_.hasKey()) {
        state_ = QStringLiteral("failed");
        readError_ = QStringLiteral("No claude_api_key in settings.json (README). Type the values in instead.");
        emit changed();
        return;
    }
    state_ = QStringLiteral("reading");
    readError_.clear();
    const int attempt = ++attempt_;
    emit changed();
    claude_.write(buildAirframeReadRequest(std::string_view(image_.constData(), static_cast<std::size_t>(image_.size())),
                                           mediaType_.toStdString()),
                  [this, attempt](int status, QByteArray body, QString networkError) {
                      if (attempt != attempt_) return;
                      if (status == 0) {
                          state_ = QStringLiteral("failed");
                          readError_ = QStringLiteral("Couldn't reach Claude: %1").arg(networkError);
                      } else if (const AirframeReadAnswer a = parseAirframeReadResponse(status, body.toStdString());
                                 a.reading) {
                          reading_ = *a.reading;
                          state_ = QStringLiteral("read");
                          refreshLayouts();
                      } else {
                          state_ = QStringLiteral("failed");
                          readError_ = q(a.error);
                      }
                      emit changed();
                  });
}

void AddAircraftModel::setField(const QString& key, const QString& value) {
    leaseError_.clear();
    const std::string k = key.toStdString();
    for (const TextField& f : kTextFields) {
        if (k == f.key) {
            std::string text = value.trimmed().toStdString();
            if (k == "registration" || k == "baseType") text = value.trimmed().toUpper().toStdString();
            reading_.*f.member = text;
        }
    }
    for (const NumberField& f : kNumberFields) {
        if (k != f.key) continue;
        QString digits = value;
        digits.remove(',').remove(' ');
        bool ok = false;
        const int n = digits.toInt(&ok);
        reading_.*f.member = ok && n >= 0 ? std::optional<int>(n) : std::nullopt;
    }
    if (k == "baseType" || k == "maxPassengers") refreshLayouts();
    emit changed();
}

void AddAircraftModel::refreshLayouts() {
    layouts_ = candidateLayouts(catalog_, reading_.baseType, reading_.maxPassengers.value_or(-1));
    const auto pick = preselectedLayout(layouts_);
    layoutIndex_ = pick ? static_cast<int>(*pick) : -1;
}

const CatalogType* AddAircraftModel::type() const { return layout() ? catalog_.type(layout()->typeCode) : nullptr; }

const CatalogAirframe* AddAircraftModel::layout() const {
    if (layoutIndex_ < 0 || layoutIndex_ >= static_cast<int>(layouts_.size())) return nullptr;
    return layouts_[static_cast<std::size_t>(layoutIndex_)].layout;
}

void AddAircraftModel::setLayoutIndex(int index) {
    if (index < 0 || index >= static_cast<int>(layouts_.size()) || !layouts_[static_cast<std::size_t>(index)].seatsMatch) {
        return;
    }
    layoutIndex_ = index;
    leaseError_.clear();
    emit changed();
}

void AddAircraftModel::setFlightNumber(const QString& number) {
    flightNumber_ = number.trimmed();
    leaseError_.clear();
    emit changed();
}

void AddAircraftModel::setDelivery(const QString& delivery) {
    delivery_ = delivery == "instant" ? QStringLiteral("instant") : QStringLiteral("factory");
    leaseError_.clear();
    emit changed();
}

void AddAircraftModel::setDeliveryIcao(const QString& icao) {
    deliveryIcao_ = icao.trimmed().toUpper();
    leaseError_.clear();
    emit changed();
}

QVariantList AddAircraftModel::fields() const {
    QVariantList out;
    for (const TextField& f : kTextFields) {
        const std::string& value = reading_.*f.member;
        out.push_back(QVariantMap{{"key", f.key}, {"label", f.label}, {"value", q(value)},
                                  {"missing", f.required && value.empty()}, {"unit", ""}});
    }
    for (const NumberField& f : kNumberFields) {
        const std::optional<int>& value = reading_.*f.member;
        out.push_back(QVariantMap{{"key", f.key}, {"label", f.label},
                                  {"value", value ? q(formatNumber(*value)) : QString()}, {"missing", !value},
                                  {"unit", f.unit}});
    }
    return out;
}

QVariantList AddAircraftModel::checks() const {
    if (state_ == "empty" || state_ == "reading") return {};
    QVariantList out;
    const QString base = q(reading_.baseType);
    if (base.isEmpty()) {
        out.push_back(QVariantMap{{"ok", false}, {"text", "Base type not read yet"}});
    } else if (layouts_.empty()) {
        out.push_back(QVariantMap{{"ok", false}, {"text", QStringLiteral("No supported aircraft has base type %1").arg(base)}});
    } else {
        QStringList types;
        for (const LayoutChoice& c : layouts_) {
            if (!types.contains(q(c.type->code))) types.push_back(q(c.type->code));
        }
        out.push_back(QVariantMap{
            {"ok", true},
            {"text", QStringLiteral("%1 is a %2, which BlockTime supports").arg(base, types.join(QStringLiteral(" or ")))}});
        const bool seats = std::any_of(layouts_.begin(), layouts_.end(), [](const LayoutChoice& c) { return c.seatsMatch; });
        const QString passengers = reading_.maxPassengers ? QString::number(*reading_.maxPassengers) : QStringLiteral("?");
        out.push_back(QVariantMap{{"ok", seats},
                                  {"text", seats ? QStringLiteral("A layout has SimBrief's %1 seats").arg(passengers)
                                                 : QStringLiteral("No layout has SimBrief's %1 seats").arg(passengers)}});
    }
    return out;
}

QVariantList AddAircraftModel::layouts() const {
    QVariantList out;
    for (const LayoutChoice& c : layouts_) {
        QString detail = seatsText(*c.layout);
        if (!c.seatsMatch && reading_.maxPassengers) detail += QStringLiteral("; SimBrief says %1").arg(*reading_.maxPassengers);
        out.push_back(QVariantMap{{"name", q(c.layout->name)}, {"detail", detail}, {"enabled", c.seatsMatch}});
    }
    return out;
}

QVariantList AddAircraftModel::holds() const {
    QVariantList out;
    if (!layout() || !reading_.maxCargoLb) return out;
    for (const Hold& h : holdsForMaxCargo(layout()->holds, *reading_.maxCargoLb)) {
        out.push_back(QVariantMap{{"name", holdName(h.hold)}, {"lb", q(formatNumber(h.maxLb))}, {"share", h.maxLb}});
    }
    return out;
}

QString AddAircraftModel::holdsText() const {
    if (!layout() || !reading_.maxCargoLb) return {};
    QStringList shares;
    for (const Hold& h : layout()->holds) shares.push_back(q(formatNumber(h.maxLb)));
    return QStringLiteral("SimBrief's %1 lb Max Cargo, shared %2 like the layout")
        .arg(q(formatNumber(*reading_.maxCargoLb)), shares.join(QStringLiteral(" : ")));
}

QString AddAircraftModel::factoryText() const {
    const CatalogType* t = type();
    if (!t) return QStringLiteral("Fly it from the factory");
    db::Statement s(db_, "SELECT name FROM airport WHERE icao_code = ?");
    s.bind(1, t->factoryIcao);
    const QString name = s.next() ? q(s.text(0)) : QString();
    return name.isEmpty() ? QStringLiteral("Fly it from %1").arg(q(t->factoryIcao))
                          : QStringLiteral("Fly it from %1 (%2)").arg(name, q(t->factoryIcao));
}

QString AddAircraftModel::deliveryCheck() const {
    if (delivery_ != "instant" || deliveryIcao_.size() != 4) return {};
    const CatalogType* t = type();
    if (t && deliveryIcao_.toStdString() == t->factoryIcao) return QStringLiteral("That's the factory: delivered there free.");
    db::Statement s(db_, "SELECT name, longest_runway_ft FROM airport WHERE icao_code = ?");
    s.bind(1, deliveryIcao_.toStdString());
    if (!s.next()) return QStringLiteral("%1 isn't in the airport data.").arg(deliveryIcao_);
    QString text = QStringLiteral("%1 · longest runway %2 ft").arg(q(s.text(0)), q(formatNumber(s.integer(1))));
    if (t) {
        if (const auto nm = airportDistanceNm(db_, t->factoryIcao, deliveryIcao_.toStdString())) {
            text += QStringLiteral(" · %1 nm from %2").arg(q(formatNumber(std::llround(*nm))), q(t->factoryIcao));
        }
    }
    return text;
}

std::optional<Preview> AddAircraftModel::deliveryPrice() const {
    const CatalogType* t = type();
    if (delivery_ != "instant" || !t || !reading_.mlwLb || deliveryIcao_.toStdString() == t->factoryIcao) return std::nullopt;
    const auto nm = airportDistanceNm(db_, t->factoryIcao, deliveryIcao_.toStdString());
    if (!nm) return std::nullopt;
    return instantDeliveryPrice(*t, *nm, *reading_.mlwLb, rates_);
}

QVariantList AddAircraftModel::deliveryLines() const {
    const auto price = deliveryPrice();
    return price ? moneyLinesToQml(price->costs) : QVariantList{};
}

QString AddAircraftModel::deliveryTotalText() const {
    const auto price = deliveryPrice();
    return price ? q(formatMoneyFull(price->totalCents)) : QString();  // a cost: negative, as MoneyLines expects
}

QVariantList AddAircraftModel::leaseLines() const {
    QVariantList out;
    const CatalogType* t = type();
    if (!t) return out;
    out.push_back(QVariantMap{{"label", "First month, charged today"}, {"amount", q(formatMoneyFull(t->monthlyLeaseCents))}});
    if (const auto price = deliveryPrice()) {
        out.push_back(QVariantMap{{"label", "Instant delivery"}, {"amount", q(formatMoneyFull(-price->totalCents))}});
    }
    return out;
}

QString AddAircraftModel::cashAfterText() const {
    const CatalogType* t = type();
    if (!t) return {};
    const auto price = deliveryPrice();
    return q(formatMoneyFull(cashCents(db_) - t->monthlyLeaseCents + (price ? price->totalCents : 0)));
}

LeaseRequest AddAircraftModel::request() const {
    LeaseRequest r{reading_, type(), layout(), flightNumber_.toStdString(), std::nullopt, today_()};
    if (delivery_ == "instant") r.instantDeliveryIcao = deliveryIcao_.toStdString();
    return r;
}

QString AddAircraftModel::refusal() const {
    if (state_ == "empty") return QStringLiteral("Drop in a screenshot of the airplane's SimBrief airframe first.");
    if (state_ == "reading") return QStringLiteral("Reading the screenshot…");
    if (!leaseError_.isEmpty()) return leaseError_;
    if (delivery_ == "instant" && deliveryIcao_.isEmpty()) return QStringLiteral("Type the airport to deliver it to.");
    return q(leaseRefusal(db_, request(), rates_));
}

bool AddAircraftModel::canRetry() const { return state_ == "failed" && claude_.hasKey() && !image_.isEmpty(); }

QString AddAircraftModel::leaseButtonText() const {
    return reading_.registration.empty() ? QStringLiteral("Lease") : QStringLiteral("Lease %1").arg(q(reading_.registration));
}

bool AddAircraftModel::lease() {
    const LeaseResult result = leaseAirplane(db_, request(), rates_);
    if (!result.airplaneId) {
        leaseError_ = q(result.refusal);
        emit changed();
        return false;
    }
    const QString registration = q(reading_.registration);
    clear();
    emit leased(registration);
    return true;
}
