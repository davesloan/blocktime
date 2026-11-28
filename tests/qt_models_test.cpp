// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the Qt side: the event feed, RecorderBridge and the Flights and Fleet models. No QML and no window; the
// models are plain QObjects, so no QGuiApplication is needed.

#include <chrono>
#include <cstring>
#include <filesystem>
#include <memory>
#include <format>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <thread>

#include <gtest/gtest.h>

#include "app_event_queue.h"
#include "dashboard_header.h"
#include "event_feed_model.h"
#include "flight_watcher.h"
#include "add_aircraft_model.h"
#include "aircraft_model.h"
#include "fleet_model.h"
#include "ledger_model.h"
#include "money_lines_qml.h"
#include "flight_events.h"
#include "flight_list_model.h"
#include "new_flight_model.h"
#include "flight_settlement.h"
#include "planned_flight.h"
#include "your_flight_model.h"
#include "plan_brief.h"
#include "predictions.h"
#include "simbrief.h"
#include "accepted_breakdown.h"
#include "dispatch_model.h"
#include "inflight_model.h"
#include "preflight_model.h"
#include "replay_planned_flight.h"
#include "replay_session.h"
#include "review_writer.h"
#include "debrief_store.h"
#include "rates.h"

#include <nlohmann/json.hpp>
#include "planner.h"
#include "event_log.h"
#include "recorder_bridge.h"
#include "test_airline.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

QString feedText(RecorderBridge& b, int row) {
    return b.events()->data(b.events()->index(row), EventFeedModel::TextRole).toString();
}
QString feedKind(RecorderBridge& b, int row) {
    return b.events()->data(b.events()->index(row), EventFeedModel::KindRole).toString();
}

// Log lines in the real format (D-020).
std::string blockOut(const std::string& id, const std::string& atc, double fuelLb = 15315.6) {
    return std::format(
        R"({{"ts":"{}","type":"block_out","data":{{"atc_id":"{}","flight_id":"{}","fuel_lb":{},"lat":32.73,"lon":-117.2,"time":"{}","title":"PMDG 737-800"}}}})",
        id, atc, id, fuelLb, id);
}
std::string tel(const std::string& ts, double lat) {
    return std::format(R"({{"ts":"{}","type":"telemetry","data":{{"alt_ft":3000,"lat":{},"lon":-117.2}}}})", ts, lat);
}
std::string blockIn(const std::string& id, const std::string& ts, int blockS, double fuelLb = 12141.0) {
    return std::format(
        R"({{"ts":"{}","type":"block_in","data":{{"block_time_s":{},"flight_id":"{}","fuel_lb":{},"lat":32.75,"lon":-117.2,"time":"{}"}}}})",
        ts, blockS, id, fuelLb, ts);
}
std::string interrupted(const std::string& id, const std::string& ts, const std::string& reason) {
    return std::format(R"({{"ts":"{}","type":"flight_interrupted","data":{{"flight_id":"{}","reason":"{}"}}}})", ts,
                       id, reason);
}

class TempLog : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-qt-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    void TearDown() override { fs::remove_all(dir_); }

    void write(const std::vector<std::string>& lines, bool append = false) {
        std::ofstream out(dir_ / "events-2026-11-22.jsonl", append ? std::ios::binary | std::ios::app : std::ios::binary);
        for (const auto& l : lines) out << l << '\n';
    }

    // One complete flight per entry: tail, departure hour (UTC), block time in seconds.
    void flights(const std::vector<std::tuple<std::string, int, int>>& list) {
        std::vector<std::string> lines;
        for (const auto& [tail, hour, blockS] : list) {
            const std::string id = std::format("2026-11-22T{:02}:00:00.000Z", hour);
            lines.push_back(blockOut(id, tail));
            lines.push_back(tel(std::format("2026-11-22T{:02}:10:00.000Z", hour), 32.74));
            lines.push_back(blockIn(id, std::format("2026-11-22T{:02}:30:00.000Z", hour), blockS));
        }
        write(lines);
    }

    fs::path dir_;
};

const std::string kOut = "2026-11-22T21:28:23.001Z";

// One QCoreApplication for the tests that need queued calls delivered (the rest don't need one).
QCoreApplication& qtApp() {
    static int argc = 1;
    static char name[] = "blocktime-qt-tests";
    static char* argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    return app;
}

// Delivers queued calls until `done` is true or 5 s pass.
template <typename Pred>
bool processUntil(Pred done) {
    qtApp();
    QElapsedTimer timer;
    timer.start();
    while (!done() && timer.elapsed() < 5000) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return done();
}

}  // namespace

// --- EventFeedModel -----------------------------------------------------------------------------------------------

TEST(EventFeed, NewestFirstWithUtcTime) {
    EventFeedModel feed;
    feed.add("info", "first", QDateTime::fromString("2026-11-24T23:53:16Z", Qt::ISODate));
    feed.add("block", "second", QDateTime::currentDateTimeUtc());
    ASSERT_EQ(feed.rowCount(), 2);
    EXPECT_EQ(feed.data(feed.index(0), EventFeedModel::TextRole).toString(), "second");
    EXPECT_EQ(feed.data(feed.index(0), EventFeedModel::KindRole).toString(), "block");
    EXPECT_EQ(feed.data(feed.index(1), EventFeedModel::TimeRole).toString(), "23:53:16");
}

TEST(EventFeed, CappedAtMaxRows) {
    EventFeedModel feed;
    for (int i = 0; i < EventFeedModel::kMaxRows + 5; ++i) feed.add("info", QString::number(i), QDateTime::currentDateTimeUtc());
    ASSERT_EQ(feed.rowCount(), EventFeedModel::kMaxRows);
    EXPECT_EQ(feed.data(feed.index(0), EventFeedModel::TextRole).toString(),
              QString::number(EventFeedModel::kMaxRows + 4));
    EXPECT_EQ(feed.data(feed.index(EventFeedModel::kMaxRows - 1), EventFeedModel::TextRole).toString(), "5");
}

// --- RecorderBridge -----------------------------------------------------------------------------------------------

TEST(Bridge, EventLineLosesItsTimePrefixAndIsABlockLine) {
    RecorderBridge b;
    b.applyLine("21:28:33  BLOCK OUT  N818SX  released 21:28:23  fuel 15316 lb", false);
    EXPECT_EQ(feedText(b, 0), "BLOCK OUT N818SX released 21:28:23 fuel 15316 lb");
    EXPECT_EQ(feedKind(b, 0), "block");
}

TEST(Bridge, LineKinds) {
    RecorderBridge b;
    b.applyLine("Connected to sim.", false);
    EXPECT_EQ(feedKind(b, 0), "info");
    b.applyLine("21:31:50  FLIGHT INTERRUPTED  (sim_stopped)", false);
    EXPECT_EQ(feedKind(b, 0), "block");
    b.applyLine("WARNING: block_in could not be written to the log; retrying every 5 s.", false);
    EXPECT_EQ(feedKind(b, 0), "warn");
    b.applyLine("LOST: block_in {}", true);
    EXPECT_EQ(feedKind(b, 0), "warn");
}

TEST(Bridge, ConnectionStatesAndStaleSampleCleared) {
    RecorderBridge b;
    int changes = 0;
    QObject::connect(&b, &RecorderBridge::connectionChanged, [&] { ++changes; });
    EXPECT_EQ(b.connection(), "waiting");
    b.applyConnection(ConnectionState::SimStarting);
    b.applyConnection(ConnectionState::Connected);
    b.applyConnection(ConnectionState::Connected);  // no change, no signal
    EXPECT_EQ(b.connection(), "connected");
    EXPECT_EQ(changes, 2);

    TelemetrySample s{};
    b.applySample(s);
    EXPECT_TRUE(b.hasSample());
    b.applyConnection(ConnectionState::WaitingForSim);
    EXPECT_EQ(b.connection(), "waiting");
    EXPECT_FALSE(b.hasSample());
}

TEST(Bridge, SampleFields) {
    RecorderBridge b;
    TelemetrySample s{};
    s.parkingBrake = 1;
    s.onGround = 1;
    s.fuelTotalLb = 15315.6;
    s.indicatedAltitudeFt = 12;
    s.numEngines = 2;
    s.engCombustion[0] = 1;
    strcpy_s(s.title, "PMDG 737-800 Soxxly");
    strcpy_s(s.atcId, "N818SX");
    b.applySample(s);
    EXPECT_TRUE(b.parkingBrake());
    EXPECT_TRUE(b.onGround());
    EXPECT_DOUBLE_EQ(b.fuelLb(), 15315.6);
    EXPECT_EQ(b.enginesRunning(), 1);
    EXPECT_EQ(b.engineCount(), 2);
    EXPECT_EQ(b.aircraft(), "PMDG 737-800 Soxxly");
    EXPECT_EQ(b.atcId(), "N818SX");
}

TEST(Bridge, BlockOutThenBlockInSummary) {
    RecorderBridge b;
    int changes = 0;
    QObject::connect(&b, &RecorderBridge::flightChanged, [&] { ++changes; });
    BlockOut out;
    out.time = std::chrono::system_clock::time_point(std::chrono::milliseconds(1'795'000'000'000));
    out.fuelLb = 15315.6;
    b.applyBlockEvent(out);
    EXPECT_TRUE(b.flightOpen());
    EXPECT_EQ(b.blockOutTime().toMSecsSinceEpoch(), 1'795'000'000'000);
    EXPECT_EQ(b.blockOutTime().timeSpec(), Qt::UTC);

    BlockIn in;
    in.fuelLb = 12141.0;
    in.blockTimeS = 1612;
    b.applyBlockEvent(in);
    EXPECT_FALSE(b.flightOpen());
    EXPECT_EQ(b.lastFlightSummary(), QString::fromUtf8("Block in 0:26 · 15,316 → 12,141 lb"));
    EXPECT_EQ(changes, 2);
}

TEST(Bridge, InterruptedSummary) {
    RecorderBridge b;
    FlightInterrupted e;
    e.reason = "recorder_crash";
    b.applyBlockEvent(e);
    EXPECT_FALSE(b.flightOpen());
    EXPECT_EQ(b.lastFlightSummary(), "Interrupted (recorder_crash)");
}

TEST(Bridge, MatchArrivesAndClearsOnDisconnect) {
    RecorderBridge b;
    int changes = 0;
    QObject::connect(&b, &RecorderBridge::matchChanged, [&] { ++changes; });
    EXPECT_FALSE(b.match().has_value());
    MatchResult m;
    m.problem = MatchProblem::WrongAirport;
    m.plannedFlightId = 7;
    m.distanceM = 4200;
    b.applyConnection(ConnectionState::Connected);
    b.applyMatch(m);
    ASSERT_TRUE(b.match().has_value());
    EXPECT_EQ(b.match()->problem, MatchProblem::WrongAirport);
    EXPECT_EQ(changes, 1);
    b.applyConnection(ConnectionState::WaitingForSim);
    EXPECT_FALSE(b.match().has_value());
    EXPECT_EQ(changes, 2);
}

TEST(Bridge, RefusedReleaseIsAFeedLineAndNoFlight) {
    RecorderBridge b;
    int flightChanges = 0;
    QObject::connect(&b, &RecorderBridge::flightChanged, [&] { ++flightChanges; });
    b.applyLine("09:12:40  NOT RECORDING  wrong_airport", false);
    EXPECT_EQ(feedText(b, 0), "NOT RECORDING wrong_airport");
    EXPECT_EQ(feedKind(b, 0), "warn");
    BlockOutRefused r;
    r.match.problem = MatchProblem::WrongAirport;
    b.applyBlockEvent(r);
    EXPECT_FALSE(b.flightOpen());
    EXPECT_EQ(flightChanges, 0);
}

TEST(Bridge, BlockEventSignalCarriesTypeAndId) {
    RecorderBridge b;
    QStringList seen;
    QObject::connect(&b, &RecorderBridge::blockEvent,
                     [&](const QString& type, const QString& id) { seen << type + " " + id; });
    BlockOut out;
    out.flightId = "2026-11-23T10:30:05.120Z";
    b.applyBlockEvent(out);
    BlockOutRefused refused;
    b.applyBlockEvent(refused);
    BlockIn in;
    in.flightId = "2026-11-23T10:30:05.120Z";
    b.applyBlockEvent(in);
    FlightInterrupted gone{"2026-11-23T11:00:00.000Z", "sim_stopped"};
    b.applyBlockEvent(gone);
    EXPECT_EQ(seen, (QStringList{"block_out 2026-11-23T10:30:05.120Z", "block_out_refused ",
                                 "block_in 2026-11-23T10:30:05.120Z", "flight_interrupted 2026-11-23T11:00:00.000Z"}));
}

TEST(Bridge, ObserverPostsTheMatch) {
    qtApp();
    RecorderBridge b;
    BridgeObserver observer(b);
    MatchResult m;
    m.problem = MatchProblem::FuelBelowRecord;
    observer.onMatch(m);
    QCoreApplication::processEvents();  // the queued call from onMatch
    ASSERT_TRUE(b.match().has_value());
    EXPECT_EQ(b.match()->problem, MatchProblem::FuelBelowRecord);
}

TEST(Bridge, ErrorLinesAlsoGoToStderrAtOnce) {
    // LOST: lines come while the app is shutting down, after the event loop has ended, so the posted copy never shows.
    // The observer writes them to std::cerr (the error log in the app, D-067) on the calling thread.
    RecorderBridge b;
    BridgeObserver observer(b);
    std::ostringstream captured;
    std::streambuf* previous = std::cerr.rdbuf(captured.rdbuf());
    observer.onLine("Connected to sim.", false);
    observer.onLine("LOST: block_in {\"flight_id\":\"x\"}", true);
    std::cerr.rdbuf(previous);
    EXPECT_EQ(captured.str(), "LOST: block_in {\"flight_id\":\"x\"}\n");
}

TEST(Bridge, BlockedShowsWarning) {
    RecorderBridge b;
    b.setBlocked();
    EXPECT_EQ(b.connection(), "blocked");
    EXPECT_EQ(feedKind(b, 0), "warn");
}

// --- FlightListModel: see "The flight's life on Live" below ------------------------------------------------------

TEST(Bridge, ObserverCallsFromAnotherThreadArriveOnTheGuiThread) {
    // The recorder runs on its own thread (D-053); the observer only posts copies to the bridge.
    qtApp();
    RecorderBridge b;
    BridgeObserver observer(b);
    std::thread recorderThread([&] {
        observer.onConnection(ConnectionState::Connected);
        observer.onLine("Connected to sim.", false);
    });
    recorderThread.join();
    EXPECT_EQ(b.connection(), "waiting");  // nothing applied until the GUI thread runs its events
    ASSERT_TRUE(processUntil([&] { return b.events()->rowCount() == 1; }));
    EXPECT_EQ(b.connection(), "connected");
    EXPECT_EQ(feedText(b, 0), "Connected to sim.");
}

// --- FleetModel (D-241) ------------------------------------------------------------------------------------------

namespace {

AirplaneView fleetPlane(std::int64_t id, const std::string& registration, const std::string& layout, int fuel,
                        bool awaitingDelivery, Cents monthly) {
    AirplaneView a;
    a.airplaneId = id;
    a.registration = registration;
    a.typeCode = "737-800";
    a.airframeName = layout;
    a.locationIcao = awaitingDelivery ? "KBFI" : "KSAN";
    a.fuelOnBoardLb = fuel;
    a.awaitingDelivery = awaitingDelivery;
    a.monthlyLeaseCents = monthly;
    return a;
}

FleetState sampleFleetState() {
    FleetState s;
    AirplaneView returned = fleetPlane(3, "N100SX", "737-800 Two-class", 0, false, 23'500'000);
    returned.returnedOn = blocktime::Date{std::chrono::year{2026}, std::chrono::month{11}, std::chrono::day{25}};
    s.airplanes = {fleetPlane(2, "N916SX", "777-300ER Standard", 61000, false, 38'500'000), returned,
                   fleetPlane(1, "N814SX", "737-800 BBJ2 VIP", 12400, true, 15'000'000)};
    s.notices = {"No airport data yet."};
    s.hoursFlown = 23.4;
    s.cashCents = 398'021'500;
    s.airlineName = "Soxxly Airlines";
    s.airlineCode = "XLY";
    s.callsign = "SOXXLY";
    return s;
}

QVariant role(const FleetModel& m, int row, const char* name) {
    const auto roles = m.roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it) {
        if (it.value() == name) return m.data(m.index(row), it.key());
    }
    return {};
}

}  // namespace

TEST(FleetModel, CardsAndSlots) {
    FleetModel m(sampleFleetState());
    EXPECT_EQ(m.rowCount(), 2);  // the returned airplane has no card
    EXPECT_EQ(m.slotsOpen(), 4);
    EXPECT_EQ(m.slotsMax(), 20);
    EXPECT_EQ(m.emptySlots(), 2);
    EXPECT_DOUBLE_EQ(m.nextSlotHours(), 30);
    EXPECT_NEAR(m.nextSlotProgress(), 0.34, 1e-9);
    const QVariantList locked = m.lockedSlotHours();
    ASSERT_EQ(locked.size(), 16);
    EXPECT_EQ(locked[0].toInt(), 30);
    EXPECT_EQ(locked[1].toInt(), 40);
    EXPECT_EQ(locked.last().toInt(), 180);
    EXPECT_EQ(m.hoursText(), "23.4 h");
    EXPECT_EQ(m.cashText(), "$3,980,215");
    EXPECT_EQ(m.leasesText(), "$535,000");  // the two in the fleet
    EXPECT_EQ(m.airlineName(), "Soxxly Airlines");
    EXPECT_EQ(m.notices(), QStringList{"No airport data yet."});
}

TEST(FleetModel, TitleLeadsWithLayoutInLeaseOrder) {
    FleetModel m(sampleFleetState());
    EXPECT_EQ(role(m, 0, "registration").toString(), "N814SX");  // leased first
    EXPECT_EQ(role(m, 0, "title").toString(), "737-800 BBJ2 VIP");
    EXPECT_EQ(role(m, 0, "place").toString(), "At KBFI \u00b7 12,400 lb");
    EXPECT_EQ(role(m, 1, "registration").toString(), "N916SX");
    EXPECT_EQ(role(m, 1, "title").toString(), "777-300ER Standard");
}

TEST(FleetModel, Buttons) {
    FleetState s = sampleFleetState();
    {
        FleetModel m(s);
        EXPECT_EQ(role(m, 0, "button").toString(), "Fly Delivery");
        EXPECT_EQ(role(m, 0, "glyph").toString(), "delivery");
        EXPECT_TRUE(role(m, 0, "buttonEnabled").toBool());
        EXPECT_EQ(role(m, 1, "button").toString(), "Fly");
        EXPECT_EQ(role(m, 1, "glyph").toString(), "ready");
        EXPECT_EQ(role(m, 1, "leg").toString(), "");
    }
    s.plannedRegistration = "N916SX";
    s.plannedLeg = "XLY 12 \u00b7 KSAN \u2192 KLAX";
    FleetModel m(s);
    EXPECT_EQ(role(m, 1, "button").toString(), "Continue Flight");
    EXPECT_EQ(role(m, 1, "glyph").toString(), "current");
    EXPECT_TRUE(role(m, 1, "buttonEnabled").toBool());
    EXPECT_EQ(role(m, 1, "leg").toString(), "XLY 12 \u00b7 KSAN \u2192 KLAX");
    EXPECT_EQ(role(m, 0, "button").toString(), "Fly Delivery");
    EXPECT_FALSE(role(m, 0, "buttonEnabled").toBool());  // one flight at a time
}

TEST(FleetModel, ResetEmitsChanged) {
    FleetModel m(sampleFleetState());
    int changes = 0;
    QObject::connect(&m, &FleetModel::changed, [&] { ++changes; });
    FleetState s = sampleFleetState();
    s.hoursFlown = 31;
    m.reset(s);
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(m.slotsOpen(), 5);
    EXPECT_EQ(m.emptySlots(), 3);
}

TEST(FleetModel, AllTwentySlotsHaveNoNextSlot) {
    FleetState s = sampleFleetState();
    s.hoursFlown = 250;
    FleetModel m(s);
    EXPECT_EQ(m.slotsOpen(), 20);
    EXPECT_TRUE(m.lockedSlotHours().isEmpty());
    EXPECT_DOUBLE_EQ(m.nextSlotProgress(), 1.0);
}

// --- LedgerModel --------------------------------------------------------------------------------------------------

namespace {

QVariant ledgerData(const LedgerModel& m, int row, const char* role) {
    return m.data(m.index(row), m.roleNames().key(role));
}

std::vector<LedgerView> sampleLedger() {
    return {LedgerView{"2026-11-22T00:00:00Z", "Lease N814SX, 22 Nov – 21 Dec", -90'000'000},
            LedgerView{"2026-11-22T00:00:00Z", "Opening cash", 500'000'000}};
}

}  // namespace

TEST(Ledger, RowsAndCash) {
    LedgerModel m(sampleLedger(), 410'000'000);
    ASSERT_EQ(m.rowCount(), 2);
    EXPECT_EQ(ledgerData(m, 0, "date").toString(), "22 Nov 2026");
    EXPECT_EQ(ledgerData(m, 0, "text").toString(), QString::fromUtf8("Lease N814SX, 22 Nov – 21 Dec"));
    EXPECT_EQ(ledgerData(m, 0, "amount").toString(), "-$900,000");
    EXPECT_FALSE(ledgerData(m, 0, "income").toBool());
    EXPECT_EQ(ledgerData(m, 1, "amount").toString(), "$5,000,000");
    EXPECT_TRUE(ledgerData(m, 1, "income").toBool());
    EXPECT_EQ(m.cashShort(), "$4.1M");
    EXPECT_EQ(m.cashFull(), "$4,100,000");
}

TEST(Ledger, ResetEmitsChanged) {
    LedgerModel m({}, 0);
    EXPECT_EQ(m.rowCount(), 0);
    EXPECT_EQ(m.cashShort(), "$0");
    int changes = 0;
    QObject::connect(&m, &LedgerModel::changed, [&] { ++changes; });
    m.reset(sampleLedger(), 410'000'000);
    EXPECT_EQ(m.rowCount(), 2);
    EXPECT_EQ(m.cashFull(), "$4,100,000");
    EXPECT_EQ(changes, 1);
}

// --- Milestone 5: New Flight and the planned flight -----------------------------------------------------------------

namespace {

std::string dataFile(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Answers at once with canned values and remembers what it was asked to open.
class FakePlanner : public Planner {
public:
    AtisRunways runways{"14R", "27"};
    QString runwayNote;
    QByteArray ofpBody = QByteArray::fromStdString(dataFile("ofp_v2_min.json"));
    QString ofpError;
    QStringList opened;
    int ofpFetches = 0;

    bool deferRunways = false;  // keep the callback, to answer later like a slow SayIntentions
    std::function<void(AtisRunways, QString)> pendingRunways;

    void fetchRunways(QString, QString, std::function<void(AtisRunways, QString)> done) override {
        if (deferRunways) {
            pendingRunways = std::move(done);
            return;
        }
        done(runways, runwayNote);
    }
    void fetchLatestOfp(std::function<void(QByteArray, QString)> done) override {
        ++ofpFetches;
        done(ofpBody, ofpError);
    }
    void openUrl(const QString& url) override { opened << url; }

    // The flight's SayIntentions calls (FlightWatcher, D-123).
    QByteArray wxBody = QByteArray::fromStdString(dataFile("getwx_live.json"));
    QString wxError;
    QStringList wxCalls;
    QByteArray commsBody = QByteArray::fromStdString(dataFile("comms_history.json"));
    QString commsError;
    int commsCalls = 0;
    void fetchWx(QString icao, std::function<void(QByteArray, QString)> done) override {
        wxCalls << icao;
        done(wxBody, wxError);
    }
    void fetchCommsHistory(std::function<void(QByteArray, QString)> done) override {
        ++commsCalls;
        done(commsBody, commsError);
    }

    // The arrival gate's calls (milestone 9, D-141).
    QByteArray siFlightBody;
    QString siFlightError = QStringLiteral("SayIntentions isn't running");
    int siFlightCalls = 0;
    QStringList assignCalls;
    QByteArray assignBody = R"({"assigned_gate_name":"E7"})";
    QString assignError;
    QByteArray parkingBody = R"({"parking":{"id":"1","name":"Gate E7","lat":36.0,"lon":-115.0,"heading":0}})";
    int parkingCalls = 0;
    void fetchSiFlight(std::function<void(QByteArray, QString)> done) override {
        ++siFlightCalls;
        done(siFlightBody, siFlightError);
    }
    void assignGate(QString gate, QString icao, std::function<void(QByteArray, QString)> done) override {
        assignCalls << gate + "@" + icao;
        done(assignError.isEmpty() ? assignBody : QByteArray(), assignError);
    }
    void fetchParking(std::function<void(QByteArray, QString)> done) override {
        ++parkingCalls;
        done(parkingBody, {});
    }
};

db::Database deliveredAirline() {
    db::Database db = test::testAirline();
    EXPECT_TRUE(db.exec("UPDATE airplane SET awaiting_delivery = 0 WHERE registration = 'N814SX'"));
    return db;
}

// A VIP flight N814SX KBFI-KSAN planned at `plannedAt`, written straight to the database.
const std::filesystem::path kRatesFile = BLOCKTIME_SOURCE_DIR "/config/rates.yaml";
const std::filesystem::path kNoLog = std::filesystem::temp_directory_path() / "blocktime-qt-test-no-log";
blocktime::FlightTargetSlot g_target;  // the planning tests don't look at it

void planVip(db::Database& db, const std::string& plannedAt) {
    NewPlannedFlight f;
    f.airplaneId = loadFlightAirplane(db, "N814SX")->airplaneId;
    f.originIcao = "KBFI";
    f.destinationIcao = "KSAN";
    f.flightNumber = "8";
    f.kind = FlightKind::Vip;
    f.cabins = {{"economy", 46}};
    f.fuelPriceCentsPer100Lb = 4100;
    f.plannedAt = plannedAt;
    f.originRunway = "14R";
    f.destinationRunway = "27";
    std::string error;
    ASSERT_TRUE(createPlannedFlight(db, f, &error).has_value()) << error;
}

}  // namespace

TEST(NewFlightModel, PlanWritesTheFlightAndOpensDispatch) {
    db::Database db = deliveredAirline();
    FakePlanner planner;
    NewFlightModel m(db, "XLY", kRatesFile, planner);
    ASSERT_TRUE(m.open("N814SX"));
    EXPECT_EQ(m.kinds(), (QStringList{"vip", "charter"}));
    EXPECT_EQ(m.flightNumber(), "8");
    m.setKind("vip");
    m.setDestination("KSAN");
    EXPECT_TRUE(m.canPlan());
    EXPECT_EQ(m.endsUpText(), "N814SX ends up at KSAN");
    EXPECT_FALSE(m.incomeLines().isEmpty());
    EXPECT_EQ(m.fuelNote(), "+ fuel");
    int planned = 0;
    QObject::connect(&m, &NewFlightModel::planned, [&] { ++planned; });
    m.plan();
    EXPECT_EQ(planned, 1);
    EXPECT_FALSE(m.busy());
    const auto f = currentPlannedFlight(db);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->destination, "KSAN");
    EXPECT_EQ(f->flightNumber, "8");
    ASSERT_EQ(planner.opened.size(), 1);
    EXPECT_TRUE(planner.opened[0].contains("&origrwy=14R&destrwy=27")) << planner.opened[0].toStdString();
    EXPECT_TRUE(planner.opened[0].contains("airline=XLY&fltnum=8")) << planner.opened[0].toStdString();
}

TEST(NewFlightModel, PlanWithoutAtisStillPlans) {
    db::Database db = deliveredAirline();
    FakePlanner planner;
    planner.runways = {};
    planner.runwayNote = "No SayIntentions keys in settings.json";
    NewFlightModel m(db, "XLY", kRatesFile, planner);
    ASSERT_TRUE(m.open("N814SX"));
    m.setDestination("KSAN");
    m.plan();
    ASSERT_EQ(planner.opened.size(), 1);
    EXPECT_FALSE(planner.opened[0].contains("origrwy")) << planner.opened[0].toStdString();
    const auto f = currentPlannedFlight(db);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->originRunway, "");
    EXPECT_EQ(f->destinationRunway, "");
}

TEST(NewFlightModel, MissingRatesFileDisablesPlan) {
    db::Database db = deliveredAirline();
    FakePlanner planner;
    NewFlightModel m(db, "XLY", std::filesystem::path(BLOCKTIME_SOURCE_DIR) / "no-such-rates.yaml", planner);
    ASSERT_TRUE(m.open("N814SX"));
    m.setDestination("KSAN");
    EXPECT_FALSE(m.canPlan());
    EXPECT_TRUE(m.error().contains("no-such-rates.yaml")) << m.error().toStdString();
}

TEST(NewFlightModel, DestinationMessageUpdates) {
    db::Database db = deliveredAirline();
    FakePlanner planner;
    NewFlightModel m(db, "XLY", kRatesFile, planner);
    ASSERT_TRUE(m.open("N814SX"));
    EXPECT_EQ(m.destinationMessage(), "");  // nothing typed yet
    m.setDestination("KSHT");
    EXPECT_EQ(m.destinationMessage(), "KSHT's longest runway is 5,000 ft; N814SX needs 7,000 ft.");
    EXPECT_FALSE(m.canPlan());
    m.setDestination("KSAN");
    EXPECT_EQ(m.destinationMessage(), "");
    m.setFlightNumber("");
    EXPECT_FALSE(m.canPlan());
}

TEST(NewFlightModel, DeliveryOffersOnlyTheDeliveryFlight) {
    db::Database db = test::testAirline();
    FakePlanner planner;
    NewFlightModel m(db, "XLY", kRatesFile, planner);
    ASSERT_TRUE(m.open("N814SX"));
    EXPECT_EQ(m.kinds(), (QStringList{"delivery"}));
    EXPECT_EQ(m.loadLines(), (QStringList{"Delivery flight: no passengers or cargo"}));
    m.setDestination("KSAN");
    EXPECT_TRUE(m.incomeLines().isEmpty());
    EXPECT_TRUE(m.totalText().startsWith("-$")) << m.totalText().toStdString();
}

TEST(YourFlightModel, ImportStoresMatchingPlan) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    YourFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    EXPECT_TRUE(m.hasFlight());
    EXPECT_EQ(m.state(), "waitingForPlan");
    EXPECT_EQ(m.title(), "XLY8 · KBFI → KSAN");
    EXPECT_EQ(m.runwayText(), "ATIS runways: 14R → 27");
    EXPECT_EQ(m.loadInText(), "Load in at KBFI, Boeing Field");
    m.importPlan();
    EXPECT_EQ(planner.ofpFetches, 1);
    EXPECT_EQ(m.state(), "waitingForSim");
    EXPECT_EQ(m.message(), "Imported the plan generated 14:02Z");
    EXPECT_EQ(m.planText(), "Plan generated 14:02Z · FL370 · 2h 31m · burn 10,900 lb");
    EXPECT_EQ(m.route(), "KBFI/14R BANGR9 BANGR DCT LKV DCT OAL J65 BLH DCT JLI SEAVU2 KSAN/27");
    EXPECT_TRUE(currentPlannedFlight(db)->plan.has_value());
}

// Review 4.4: income and costs reach the panel apart, each with its working.
TEST(YourFlightModel, EstimateSplitsIncomeAndCosts) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    YourFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.importPlan();
    ASSERT_FALSE(m.estimateIncome().isEmpty());
    ASSERT_FALSE(m.estimateCosts().isEmpty());
    for (const QVariant& v : m.estimateIncome()) EXPECT_FALSE(v.toMap()["amount"].toString().startsWith("-"));
    bool fuel = false;
    for (const QVariant& v : m.estimateCosts()) {
        const QVariantMap line = v.toMap();
        EXPECT_TRUE(line["amount"].toString().startsWith("-")) << line["label"].toString().toStdString();
        EXPECT_FALSE(line["working"].toString().isEmpty()) << line["label"].toString().toStdString();
        if (line["category"] == "fuel") fuel = true;
    }
    EXPECT_TRUE(fuel);  // the plan's in, and the airplane holds less than its ramp fuel
}

TEST(YourFlightModel, ImportRefusesAStalePlan) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:20:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    YourFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.importPlan();
    EXPECT_EQ(m.state(), "waitingForPlan");
    EXPECT_TRUE(m.message().startsWith("The latest SimBrief plan was generated at 14:02Z")) << m.message().toStdString();
    EXPECT_FALSE(currentPlannedFlight(db)->plan.has_value());
}

TEST(YourFlightModel, ImportShowsSimbriefError) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    planner.ofpBody = R"({"fetch":{"userid":"","static_id":"","status":"Error: Unknown UserID","time":"0"}})";
    RecorderBridge bridge;
    YourFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.importPlan();
    EXPECT_TRUE(m.message().contains("Error: Unknown UserID")) << m.message().toStdString();
    EXPECT_EQ(m.state(), "waitingForPlan");
}

TEST(YourFlightModel, ImportShowsNetworkError) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    planner.ofpBody.clear();
    planner.ofpError = "Add simbrief_user_id to settings.json";
    RecorderBridge bridge;
    YourFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.importPlan();
    EXPECT_EQ(m.message(), "Add simbrief_user_id to settings.json");
}

TEST(YourFlightModel, CancelClears) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    YourFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    int changed = 0;
    QObject::connect(&m, &YourFlightModel::flightChanged, [&] { ++changed; });
    m.cancelFlight();
    EXPECT_FALSE(m.hasFlight());
    EXPECT_EQ(changed, 1);
    EXPECT_FALSE(currentPlannedFlight(db).has_value());
}

TEST(YourFlightModel, StateFollowsConnection) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    YourFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.importPlan();
    ASSERT_EQ(m.state(), "waitingForSim");
    bridge.applyConnection(ConnectionState::Connected);
    EXPECT_EQ(m.state(), "simConnected");
    EXPECT_EQ(m.stateText(), "Sim connected");
}

TEST(YourFlightModel, OpenDispatchPageRebuildsTheUrl) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    YourFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.openDispatchPage();
    ASSERT_EQ(planner.opened.size(), 1);
    EXPECT_TRUE(planner.opened[0].contains("orig=KBFI&dest=KSAN&airline=XLY&fltnum=8")) << planner.opened[0].toStdString();
    EXPECT_TRUE(planner.opened[0].contains("&pax=46&")) << planner.opened[0].toStdString();
}

TEST(FleetModel, RegistrationAtRow) {
    FleetModel m(sampleFleetState());
    EXPECT_EQ(m.registrationAt(0), "N814SX");  // in lease order
    EXPECT_EQ(m.registrationAt(1), "N916SX");
    EXPECT_EQ(m.registrationAt(5), "");
}

TEST(NewFlightModel, CancelWhileGettingRunwaysPlansNothing) {  // review finding 1
    db::Database db = deliveredAirline();
    FakePlanner planner;
    planner.deferRunways = true;
    NewFlightModel m(db, "XLY", kRatesFile, planner);
    ASSERT_TRUE(m.open("N814SX"));
    m.setDestination("KSAN");
    int planned = 0;
    QObject::connect(&m, &NewFlightModel::planned, [&] { ++planned; });
    m.plan();
    EXPECT_TRUE(m.busy());
    m.cancel();
    EXPECT_FALSE(m.busy());
    ASSERT_TRUE(planner.pendingRunways);
    planner.pendingRunways(planner.runways, {});
    EXPECT_EQ(planned, 0);
    EXPECT_TRUE(planner.opened.isEmpty());
    EXPECT_FALSE(currentPlannedFlight(db).has_value());
}

TEST(NewFlightModel, ReopeningWhileGettingRunwaysPlansNothing) {  // review finding 1
    db::Database db = deliveredAirline();
    FakePlanner planner;
    planner.deferRunways = true;
    NewFlightModel m(db, "XLY", kRatesFile, planner);
    ASSERT_TRUE(m.open("N814SX"));
    m.setDestination("KSAN");
    m.plan();
    ASSERT_TRUE(m.open("N738BT"));
    planner.pendingRunways(planner.runways, {});
    EXPECT_TRUE(planner.opened.isEmpty());
    EXPECT_FALSE(currentPlannedFlight(db).has_value());
    EXPECT_EQ(m.error(), "");
}

// --- The flight's life on Live (milestone 6) ----------------------------------------------------------------------

namespace {

constexpr const char* kLivePlannedAt = "2026-11-22T10:00:00Z";
constexpr const char* kLiveOut = "2026-11-22T10:30:05.120Z";
constexpr const char* kLiveIn = "2026-11-22T13:20:41.900Z";

std::string liveLine(const std::string& type, const std::string& data) {
    return R"({"ts":"2026-11-22T00:00:00.000Z","type":")" + type + R"(","data":)" + data + "}";
}

std::string liveOut(std::int64_t planned) {
    return liveLine("block_out", std::string(R"({"flight_id":")") + kLiveOut + R"(","time":")" + kLiveOut +
                                     R"(","lat":47.53,"lon":-122.302,"fuel_lb":20000,"planned_flight_id":)" +
                                     std::to_string(planned) + "}");
}

std::string liveIn() {
    return liveLine("block_in", std::string(R"({"flight_id":")") + kLiveOut + R"(","time":")" + kLiveIn +
                                    R"(","lat":32.7336,"lon":-117.19,"fuel_lb":9500,"shutdown_fuel_lb":9400,)" +
                                    R"("block_time_s":10236})");
}

class LiveFlight : public TempLog {
protected:
    void SetUp() override {
        TempLog::SetUp();
        NewPlannedFlight f;
        f.airplaneId = loadFlightAirplane(db_, "N814SX")->airplaneId;
        f.originIcao = "KBFI";
        f.destinationIcao = "KSAN";
        f.flightNumber = "8";
        f.kind = FlightKind::Delivery;
        f.fuelPriceCentsPer100Lb = 4100;
        f.plannedAt = kLivePlannedAt;
        std::string error;
        const auto id = createPlannedFlight(db_, f, &error);
        ASSERT_TRUE(id.has_value()) << error;
        plannedId_ = *id;
        // A flight is only recorded with a plan imported (D-250).
        SimbriefPlan p;
        p.requestId = "r1";
        p.generatedAt = "2026-11-22T10:05:00Z";
        p.units = "lbs";
        p.route = "DCT";
        p.originRunway = "14R";
        p.destinationRunway = "27";
        p.cruiseAltitudeFt = 37000;
        p.estBlockS = 10200;
        p.rampFuelLb = 20000;
        p.enrouteBurnLb = 15000;
        p.estLandingWeightLb = 120000;
        p.originTransitionAltitudeFt = 18000;
        p.destinationTransitionLevelFt = 18000;
        ASSERT_TRUE(storeSimbriefPlan(db_, plannedId_, p, "{}", "2026-11-22T10:06:00Z", &error)) << error;
    }

    std::unique_ptr<YourFlightModel> model() {
        return std::make_unique<YourFlightModel>(db_, "XLY", planner_, bridge_, target_, dir_, kRatesFile);
    }

    db::Database db_ = test::testAirline();
    FakePlanner planner_;
    RecorderBridge bridge_;
    FlightTargetSlot target_;
    std::int64_t plannedId_ = 0;
};

}  // namespace

TEST_F(LiveFlight, ReloadSetsTheTarget) {
    auto m = model();
    const auto t = target_.get();
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->plannedFlightId, plannedId_);
    EXPECT_EQ(t->plannedAt, kLivePlannedAt);
    EXPECT_EQ(t->simFolder, "PMDG 737-800BBJ2");
    EXPECT_EQ(t->typeCode, "737-800 BBJ2");
    EXPECT_EQ(t->registration, "N814SX");
    EXPECT_EQ(t->originIcao, "KBFI");
    EXPECT_DOUBLE_EQ(t->originLat, 47.53);
    EXPECT_EQ(t->fuelOnRecordLb, 0);
    m->cancelFlight();
    EXPECT_FALSE(target_.get().has_value());
}

TEST_F(LiveFlight, NotRecordingTexts) {
    auto m = model();
    EXPECT_EQ(m->notRecording(), "");  // no answer from the recorder yet
    bridge_.applyConnection(ConnectionState::Connected);
    MatchResult r;
    r.plannedFlightId = plannedId_;
    r.problem = MatchProblem::WrongAircraftType;
    r.aircraftFolder = "pmdg 737-800";
    bridge_.applyMatch(r);
    EXPECT_EQ(m->notRecording(), "Not recording: you're in a 737-800; this flight expects the 737-800 BBJ2 N814SX.");
    r.aircraftFolder = "some other plane";
    bridge_.applyMatch(r);
    EXPECT_EQ(m->notRecording(),
              "Not recording: you're in a some other plane; this flight expects the 737-800 BBJ2 N814SX.");

    r.problem = MatchProblem::WrongAirport;
    r.aircraftFolder = "pmdg 737-800bbj2";
    r.lat = 47.45;  // KSEA's neighbourhood: the nearest imported airport in the test data is KBFI itself
    r.lon = -122.31;
    r.distanceM = 8900;
    bridge_.applyMatch(r);
    EXPECT_EQ(m->notRecording(), "Not recording: you're 8.9 km from KBFI; load in at KBFI.");
    r.lat = 32.7336;  // at KSAN
    r.lon = -117.19;
    r.distanceM = 1'500'000;
    bridge_.applyMatch(r);
    EXPECT_EQ(m->notRecording(), "Not recording: you're at KSAN; load in at KBFI.");

    ASSERT_TRUE(db_.exec("UPDATE airplane SET fuel_on_board_lb = 5000 WHERE registration = 'N814SX'"));
    m->reload();
    r.problem = MatchProblem::FuelBelowRecord;
    r.minFuelLb = 4700;
    bridge_.applyMatch(r);
    EXPECT_EQ(m->notRecording(),
              "Not recording: load at least 4,700 lb of fuel (N814SX had 5,000 lb at its last block in).");

    r.problem = MatchProblem::None;
    bridge_.applyMatch(r);
    EXPECT_EQ(m->notRecording(), "");
    r.problem = MatchProblem::WrongAirport;
    bridge_.applyMatch(r);
    bridge_.applyBlockEvent(BlockOut{});  // in flight: nothing to say
    EXPECT_EQ(m->notRecording(), "");
}

TEST_F(LiveFlight, NoPlannedFlightText) {
    std::string error;
    ASSERT_TRUE(cancelPlannedFlight(db_, &error)) << error;
    auto m = model();
    bridge_.applyConnection(ConnectionState::Connected);
    MatchResult r;
    r.problem = MatchProblem::NoPlannedFlight;
    bridge_.applyMatch(r);
    EXPECT_EQ(m->notRecording(), "Not recording: no flight is planned. Plan one from Fleet.");
}

TEST_F(LiveFlight, SettlesOnBlockIn) {
    auto m = model();
    int settled = 0;
    QObject::connect(m.get(), &YourFlightModel::settled, [&] { ++settled; });
    write({liveOut(plannedId_)});
    BlockOut out;
    out.flightId = kLiveOut;
    bridge_.applyBlockEvent(out);
    EXPECT_EQ(m->state(), "flying");
    write({liveIn()}, true);
    BlockIn in;
    in.flightId = kLiveOut;
    bridge_.applyBlockEvent(in);
    EXPECT_EQ(settled, 1);
    EXPECT_FALSE(m->hasFlight());
    EXPECT_EQ(m->state(), "summary");
    EXPECT_EQ(m->summaryFlightId(), kLiveOut);
    EXPECT_FALSE(target_.get().has_value());
    EXPECT_EQ(m->notice(), "");
}

TEST_F(LiveFlight, SettlesAtStartup) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    int settled = 0;
    std::size_t handed = 0;  // the lines settlement read, handed on to Claude's review (review 5.1)
    QObject::connect(m.get(), &YourFlightModel::settled, [&] {
        ++settled;
        handed = m->takeSettledLines().size();
    });
    m->checkFlown();
    EXPECT_EQ(settled, 1);
    EXPECT_EQ(handed, 2u);
    EXPECT_TRUE(m->takeSettledLines().empty());  // taken once
    EXPECT_EQ(m->summaryFlightId(), kLiveOut);
    m->checkFlown();  // nothing left to settle
    EXPECT_EQ(settled, 1);
}

// Review 1.2: a block in whose log line was held (D-049) leaves the flight flown but not settled. It can't be
// cancelled, Live says why, and the minute tick settles it once the line is in the log.
TEST_F(LiveFlight, HeldBlockInWaitsToSettleAndCantBeCancelled) {
    auto m = model();
    int settled = 0;
    QObject::connect(m.get(), &YourFlightModel::settled, [&] { ++settled; });
    write({liveOut(plannedId_)});
    BlockOut out;
    out.flightId = kLiveOut;
    bridge_.applyBlockEvent(out);
    BlockIn in;
    in.flightId = kLiveOut;
    bridge_.applyBlockEvent(in);  // told, but the line isn't written yet
    EXPECT_EQ(settled, 0);
    EXPECT_TRUE(m->notice().startsWith("Flown, waiting to settle")) << m->notice().toStdString();
    m->cancelFlight();
    EXPECT_TRUE(m->hasFlight());
    m->retryPendingSettle();  // the minute tick, still not written
    EXPECT_EQ(settled, 0);
    write({liveIn()}, true);
    m->retryPendingSettle();
    EXPECT_EQ(settled, 1);
    EXPECT_FALSE(m->hasFlight());
    EXPECT_EQ(m->notice(), "");
}

// Review 1.2: whatever kept it from settling, a flight the log shows flown is never cancelled.
TEST_F(LiveFlight, FlownFlightInTheLogCantBeCancelled) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    m->cancelFlight();
    EXPECT_TRUE(m->hasFlight());
    EXPECT_TRUE(m->message().contains("flown")) << m->message().toStdString();
}

TEST_F(LiveFlight, NothingFlownIsNotPending) {
    auto m = model();
    m->checkFlown();
    EXPECT_EQ(m->notice(), "");
    m->cancelFlight();
    EXPECT_FALSE(m->hasFlight());
}

TEST_F(LiveFlight, InterruptedKeepsThePlanWithANotice) {
    auto m = model();
    write({liveOut(plannedId_), liveLine("flight_interrupted", std::string(R"({"flight_id":")") + kLiveOut +
                                                                   R"(","reason":"sim_stopped"})")});
    BlockOut out;
    out.flightId = kLiveOut;
    bridge_.applyBlockEvent(out);
    bridge_.applyBlockEvent(FlightInterrupted{kLiveOut, "sim_stopped"});
    EXPECT_TRUE(m->hasFlight());
    EXPECT_EQ(m->notice(), QString::fromUtf8("KBFI–KSAN interrupted and discarded"));
    EXPECT_EQ(m->state(), "waitingForSim");  // the fixture has a plan (D-250)
    EXPECT_TRUE(target_.get().has_value());
}

TEST_F(LiveFlight, TurnaroundAsksForNewFlight) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    m->checkFlown();
    QStringList asked;
    QObject::connect(m.get(), &YourFlightModel::turnaroundRequested, [&](const QString& r) { asked << r; });
    m->turnaround();
    EXPECT_EQ(asked, QStringList{"N814SX"});
    EXPECT_EQ(m->state(), "");
    EXPECT_EQ(m->summaryFlightId(), "");
}

TEST_F(LiveFlight, DoneForNowShowsNextLeg) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    BlockOut out;
    out.flightId = kLiveOut;
    bridge_.applyBlockEvent(out);
    EXPECT_TRUE(bridge_.showingFlight());
    m->checkFlown();
    EXPECT_TRUE(bridge_.showingFlight());  // the summary shows with the flight's Arrival under it
    m->doneForNow();
    EXPECT_FALSE(bridge_.showingFlight());
}

TEST_F(LiveFlight, TurnaroundShowsNextLeg) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    BlockOut out;
    out.flightId = kLiveOut;
    bridge_.applyBlockEvent(out);
    m->checkFlown();
    m->turnaround();
    EXPECT_FALSE(bridge_.showingFlight());
}

TEST_F(LiveFlight, DeboardingRecordedWhenItEnds) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    m->checkFlown();
    write({liveLine("gsx_service", R"({"service":"deboarding","state":"performing","time":"2026-11-23T13:22:00.000Z"})"),
           liveLine("gsx_service", R"({"service":"deboarding","state":"completed","time":"2026-11-23T13:28:00.000Z"})")},
          true);
    bridge_.applyFlightEvent("gsx_service",
                             {{"service", "deboarding"}, {"state", "completed"}, {"time", "2026-11-23T13:28:00.000Z"}});
    db::Statement q(db_, "SELECT COUNT(*) FROM flight_service WHERE service = 'deboarding'");
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.integer(0), 1);
}

TEST_F(LiveFlight, DoneForNowClearsTheSummary) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    m->checkFlown();
    m->doneForNow();
    EXPECT_EQ(m->state(), "");
}

// --- Flights from the database (D-111) ----------------------------------------------------------------------------

TEST_F(LiveFlight, FlightListRowsFromDatabaseAndTrackFromLog) {
    write({liveOut(plannedId_), tel("2026-11-22T11:00:00.000Z", 45.0), tel("2026-11-22T12:00:00.000Z", 38.0),
           liveIn()});
    auto m = model();
    m->checkFlown();
    qtApp();
    FlightListModel list(db_, dir_, "XLY");
    list.reloadBlocking();
    ASSERT_EQ(list.count(), 1);
    const QModelIndex i = list.index(0);
    EXPECT_EQ(list.data(i, FlightListModel::FlightIdRole).toString(), kLiveOut);
    EXPECT_EQ(list.data(i, FlightListModel::DateTextRole).toString(), "22 Nov 2026");
    EXPECT_EQ(list.data(i, FlightListModel::OutTextRole).toString(), "10:30Z");
    EXPECT_EQ(list.data(i, FlightListModel::InTextRole).toString(), "13:20Z");
    EXPECT_EQ(list.data(i, FlightListModel::RouteTextRole).toString(), QString::fromUtf8("KBFI–KSAN"));
    EXPECT_EQ(list.data(i, FlightListModel::FlightNumberRole).toString(), "XLY8");
    EXPECT_EQ(list.data(i, FlightListModel::RegistrationRole).toString(), "N814SX");
    EXPECT_EQ(list.data(i, FlightListModel::BlockTextRole).toString(), "2:50");
    EXPECT_EQ(list.data(i, FlightListModel::FuelUsedLbRole).toInt(), 10600);
    EXPECT_LT(list.data(i, FlightListModel::TotalCentsRole).toLongLong(), 0);  // a delivery is a loss
    EXPECT_TRUE(list.data(i, FlightListModel::TotalTextRole).toString().startsWith("-$"));
    EXPECT_GT(list.track(0).path().size(), 2);  // the logged telemetry, not just the two airports
    EXPECT_GT(list.data(i, FlightListModel::DistanceNmRole).toDouble(), 0.0);

    const QVariantMap summary = list.summary(0);
    EXPECT_EQ(summary["title"].toString(), QString::fromUtf8("XLY8 · KBFI → KSAN"));
    EXPECT_EQ(summary["routeText"].toString(), QString::fromUtf8("N814SX · 737-800 BBJ2 · Delivery flight"));
    EXPECT_EQ(summary["blockText"].toString(), "2:50");
    EXPECT_EQ(summary["fuelText"].toString(), "10,600 lb used");
    EXPECT_TRUE(summary["income"].toList().isEmpty());
    const QVariantList costs = summary["costs"].toList();
    ASSERT_EQ(costs.size(), 5);
    EXPECT_EQ(costs[0].toMap()["label"].toString(), "Fuel");
    EXPECT_EQ(costs[0].toMap()["amount"].toString(), "-$8,200");
    EXPECT_TRUE(summary["totalText"].toString().startsWith("-$"));
    EXPECT_EQ(list.rowOf(kLiveOut), 0);
    EXPECT_EQ(list.rowOf("nope"), -1);
}

// The Replay button (D-175) is greyed, with the reason, once the flight's log is gone.
TEST_F(LiveFlight, FlightListReplayNote) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    m->checkFlown();
    FlightListModel list(db_, dir_, "XLY");
    list.reloadBlocking();
    ASSERT_EQ(list.count(), 1);
    EXPECT_EQ(list.summary(0)["replayNote"].toString(), "");
    for (const auto& f : fs::directory_iterator(dir_)) fs::remove(f.path());
    EXPECT_EQ(list.summary(0)["replayNote"].toString(), "The log for 22 Nov is no longer on this PC");
}

TEST_F(LiveFlight, FlightListShowsADiversion) {
    write({liveOut(plannedId_),
           liveLine("block_in", std::string(R"({"flight_id":")") + kLiveOut + R"(","time":")" + kLiveIn +
                                    R"(","lat":47.0,"lon":-120.0,"shutdown_fuel_lb":9400,"block_time_s":10236})")});
    auto m = model();
    m->checkFlown();
    FlightListModel list(db_, dir_, "XLY");
    list.reloadBlocking();
    ASSERT_EQ(list.count(), 1);
    EXPECT_EQ(list.data(list.index(0), FlightListModel::RouteTextRole).toString(),
              QString::fromUtf8("KBFI–KSAN, diverted to KSHT"));
    EXPECT_EQ(list.summary(0)["title"].toString(), QString::fromUtf8("XLY8 · KBFI → KSHT (planned KSAN)"));
}

TEST_F(LiveFlight, FlightListWithoutALogTrackShowsTheTwoAirports) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    m->checkFlown();
    fs::remove_all(dir_);  // the log is gone (older than 30 days, say)
    fs::create_directories(dir_);
    FlightListModel list(db_, dir_, "XLY");
    list.reloadBlocking();
    ASSERT_EQ(list.count(), 1);
    const QGeoPath track = list.track(0);
    ASSERT_EQ(track.path().size(), 2);
    EXPECT_DOUBLE_EQ(track.path()[0].latitude(), 47.53);
    EXPECT_DOUBLE_EQ(track.path()[1].latitude(), 32.7336);
    EXPECT_LT(list.data(list.index(0), FlightListModel::DistanceNmRole).toDouble(), 0.0);  // unknown
}

TEST_F(LiveFlight, FlightListLeavesOutInterruptedFlights) {
    write({liveOut(plannedId_), interrupted(kLiveOut, "2026-11-22T10:40:00.000Z", "sim_stopped")});
    FlightListModel list(db_, dir_, "XLY");
    list.reloadBlocking();
    EXPECT_EQ(list.count(), 0);
}

TEST_F(LiveFlight, FlightListReloadReadsRowsAtOnceAndTracksOnAWorker) {
    write({liveOut(plannedId_), tel("2026-11-22T11:00:00.000Z", 45.0), liveIn()});
    auto m = model();
    m->checkFlown();
    qtApp();
    FlightListModel list(db_, dir_, "XLY");
    EXPECT_EQ(list.count(), 0);  // construction reads nothing
    int tracksChanges = 0;
    QObject::connect(&list, &FlightListModel::tracksChanged, [&] { ++tracksChanges; });
    list.reload();
    EXPECT_EQ(list.count(), 1);  // rows come from the database straight away
    EXPECT_TRUE(list.loading());
    ASSERT_TRUE(processUntil([&] { return !list.loading(); }));
    EXPECT_EQ(tracksChanges, 1);
    EXPECT_GT(list.track(0).path().size(), 2);
}

TEST_F(LiveFlight, DestroyingTheFlightListWhileLoadingDoesNotWait) {
    write({liveOut(plannedId_), liveIn()});
    qtApp();
    {
        FlightListModel list(db_, dir_, "XLY");
        list.reload();
    }  // stops and joins the worker; the posted result is dropped with the model
    QCoreApplication::processEvents();
    SUCCEED();
}

// --- Review fixes (milestone 6) -----------------------------------------------------------------------------------

namespace {

TelemetrySample brake(bool set) {
    TelemetrySample t{};
    t.parkingBrake = set ? 1 : 0;
    t.onGround = 1;
    return t;
}

}  // namespace

TEST_F(LiveFlight, RefusedReleaseNeedsANewRelease) {  // review finding 1
    auto m = model();
    bridge_.applyConnection(ConnectionState::Connected);
    bridge_.applySample(brake(false));
    MatchResult r;
    r.plannedFlightId = plannedId_;
    r.problem = MatchProblem::FuelBelowRecord;
    r.minFuelLb = 4700;
    bridge_.applyMatch(r);
    bridge_.applyBlockEvent(BlockOutRefused{{}, r, 47.53, -122.302});
    r.problem = MatchProblem::None;  // refuelled: the sim matches now, but that release is spent
    bridge_.applyMatch(r);
    EXPECT_EQ(m->notRecording(),
              "Not recording yet: set the parking brake, then release it. The last release came before the sim "
              "matched this flight.");
    bridge_.applySample(brake(true));
    EXPECT_EQ(m->notRecording(), "");
    bridge_.applySample(brake(false));  // the new release the detector is waiting for
    EXPECT_EQ(m->notRecording(), "");
}

TEST_F(LiveFlight, UnreadableRatesDontSettle) {  // review finding 2
    write({liveOut(plannedId_), liveIn()});
    YourFlightModel m(db_, "XLY", planner_, bridge_, target_, dir_, dir_ / "no-rates.yaml");
    int settled = 0;
    QObject::connect(&m, &YourFlightModel::settled, [&] { ++settled; });
    m.checkFlown();
    EXPECT_EQ(settled, 0);
    EXPECT_TRUE(m.hasFlight());
    EXPECT_TRUE(m.notice().contains("no-rates.yaml")) << m.notice().toStdString();
    std::string error;
    EXPECT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << "still there to settle later: " << error;
}

TEST_F(LiveFlight, CancelWhileFlyingKeepsThePlan) {  // review finding 4
    auto m = model();
    BlockOut out;
    out.flightId = kLiveOut;
    bridge_.applyBlockEvent(out);
    m->cancelFlight();
    EXPECT_TRUE(m->hasFlight());
    EXPECT_EQ(m->state(), "flying");
}

TEST_F(LiveFlight, NoBannerOverTheSummary) {  // review finding 6
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    m->checkFlown();
    ASSERT_EQ(m->state(), "summary");
    bridge_.applyConnection(ConnectionState::Connected);
    MatchResult r;
    r.problem = MatchProblem::NoPlannedFlight;
    bridge_.applyMatch(r);
    EXPECT_EQ(m->notRecording(), "");
    m->doneForNow();
    EXPECT_EQ(m->notRecording(), "Not recording: no flight is planned. Plan one from Fleet.");
}

// --- FlightWatcher (D-123, D-127) ---------------------------------------------------------------------------------

namespace {

struct WatcherRig {
    FakePlanner planner;
    RecorderBridge bridge;
    blocktime::FlightTargetSlot target;
    blocktime::AppEventQueue queue;
    std::vector<blocktime::LogEvent> drained;
    std::unique_ptr<FlightWatcher> watcher;

    WatcherRig() {
        qtApp();
        blocktime::FlightTarget t;
        t.originIcao = "KBFI";
        t.destinationIcao = "KPDX";
        target.set(t);
        watcher = std::make_unique<FlightWatcher>(planner, bridge, target, queue);
        watcher->setIntervals(std::chrono::milliseconds(30), std::chrono::milliseconds(30));
        watcher->setGateInterval(std::chrono::milliseconds(30));
        watcher->setGateContext([this](const QString& icao) -> std::optional<blocktime::GateContext> {
            if (icao != "KPDX") return std::nullopt;
            return blocktime::GateContext{35.8, blocktime::SpotUse::Gate, preferred};
        });
    }
    std::vector<std::string> preferred{"E7"};  // Dave prefers E7 at KPDX (D-155)
    // KPDX with Gate E7 200 m north of the runway and Gate E9 600 m north.
    void loadDestination() {
        blocktime::AirportFacilities a;
        a.icao = "KPDX";
        a.lat = 36.0;
        a.lon = -115.0;
        a.runways = {blocktime::Runway{36.0, -115.0, 90.0, 3000.0, 45.0, 9, 0, 27, 0}};
        for (const auto& [number, north] : {std::pair{7, 200.0}, std::pair{9, 600.0}}) {
            blocktime::ParkingSpot s;
            s.name = 16;
            s.number = number;
            s.type = 9;
            s.radiusM = 20;
            blocktime::placeSpot(s, a.lat, a.lon, 0, north);
            a.spots.push_back(s);
        }
        bridge.applyAirport(a);
    }
    // SayIntentions has picked Gate E9 (600 m north).
    void sayIntentionsPicksE9() {
        planner.siFlightError.clear();
        planner.siFlightBody =
            R"({"flight_details":{"api_key":"x","current_flight":{"flight_destination":"KPDX","assigned_gate":"Gate E9",)"
            R"("assigned_gate_lat":"36.0053899","assigned_gate_lon":"-115.0","flight_plan_arriving_runway":"27"}}})";
    }
    void blockOut() {
        blocktime::BlockOut out;
        out.flightId = "2026-11-23T22:51:08.969Z";
        bridge.applyBlockEvent(out);
    }
    void flight(const QString& type, QVariantMap data = {}) { bridge.applyFlightEvent(type, data); }
    // Everything pushed so far, of one type.
    std::vector<blocktime::LogEvent> pushed(const std::string& type) {
        for (auto& e : queue.drain()) drained.push_back(std::move(e));
        std::vector<blocktime::LogEvent> out;
        for (const auto& e : drained)
            if (e.type == type) out.push_back(e);
        return out;
    }
    void wait(int ms) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < ms) QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
};

}  // namespace

TEST(FlightWatcher, PollsTheOriginUntilLiftoff) {
    WatcherRig r;
    r.blockOut();
    ASSERT_FALSE(r.planner.wxCalls.isEmpty());
    EXPECT_EQ(r.planner.wxCalls.front(), "KBFI");
    const auto atis = r.pushed("atis");
    ASSERT_EQ(atis.size(), 1u);
    EXPECT_EQ(atis[0].data["role"], "origin");
    EXPECT_EQ(atis[0].data["letter"], "K");
    EXPECT_EQ(atis[0].data["flight_id"], "2026-11-23T22:51:08.969Z");
    r.wait(100);  // more polls, same answer: nothing new
    EXPECT_GT(r.planner.wxCalls.size(), 1);
    EXPECT_EQ(r.pushed("atis").size(), 1u);
    r.flight("liftoff");
    const auto calls = r.planner.wxCalls.size();
    r.wait(100);
    EXPECT_EQ(r.planner.wxCalls.size(), calls);
}

TEST(FlightWatcher, DestinationFromTopOfDescentToTouchdown) {
    WatcherRig r;
    r.blockOut();
    r.flight("liftoff");
    r.planner.wxCalls.clear();
    r.wait(80);
    EXPECT_TRUE(r.planner.wxCalls.isEmpty());  // nothing during the cruise
    r.planner.wxBody = R"({"airports":[{"airport":"KPDX","current":"B","atis":"Portland information Bravo"}]})";
    r.flight("phase_changed", {{"phase", "descent"}});
    ASSERT_FALSE(r.planner.wxCalls.isEmpty());
    EXPECT_EQ(r.planner.wxCalls.front(), "KPDX");
    const auto atis = r.pushed("atis");
    ASSERT_EQ(atis.size(), 2u);
    EXPECT_EQ(atis[1].data["role"], "destination");
    EXPECT_EQ(atis[1].data["letter"], "B");
    r.flight("touchdown");
    const auto calls = r.planner.wxCalls.size();
    r.wait(100);
    EXPECT_EQ(r.planner.wxCalls.size(), calls);
}

// Milestone 13 (D-188): ATC and the origin's ATIS at the gate, so the clearance shows before pushback.
TEST(FlightWatcher, PreFlightPollsWhileConnected) {
    WatcherRig r;
    r.bridge.applyConnection(blocktime::ConnectionState::Connected);
    r.watcher->setPreFlightOrigin("KBFI");
    ASSERT_FALSE(r.planner.wxCalls.isEmpty());
    EXPECT_EQ(r.planner.wxCalls.front(), "KBFI");
    EXPECT_GE(r.planner.commsCalls, 1);
    const auto atis = r.pushed("atis");
    ASSERT_EQ(atis.size(), 1u);
    EXPECT_EQ(atis[0].data["role"], "origin");
    EXPECT_EQ(atis[0].data["flight_id"], "");
    const auto exchanges = r.pushed("atc_exchange");
    ASSERT_FALSE(exchanges.empty());
    EXPECT_EQ(exchanges[0].data["flight_id"], "");
    r.wait(100);
    EXPECT_GT(r.planner.wxCalls.size(), 1);
    EXPECT_GT(r.planner.commsCalls, 1);
}

TEST(FlightWatcher, PreFlightWaitsForTheSim) {
    WatcherRig r;
    r.watcher->setPreFlightOrigin("KBFI");
    r.wait(100);
    EXPECT_TRUE(r.planner.wxCalls.isEmpty());
    EXPECT_EQ(r.planner.commsCalls, 0);
    r.bridge.applyConnection(blocktime::ConnectionState::Connected);
    EXPECT_FALSE(r.planner.wxCalls.isEmpty());
    EXPECT_GE(r.planner.commsCalls, 1);
    r.bridge.applyConnection(blocktime::ConnectionState::WaitingForSim);
    const auto wx = r.planner.wxCalls.size();
    const int comms = r.planner.commsCalls;
    r.wait(100);
    EXPECT_EQ(r.planner.wxCalls.size(), wx);
    EXPECT_EQ(r.planner.commsCalls, comms);
}

TEST(FlightWatcher, PreFlightStopsWhenThePlanGoes) {
    WatcherRig r;
    r.bridge.applyConnection(blocktime::ConnectionState::Connected);
    r.watcher->setPreFlightOrigin("KBFI");
    r.watcher->setPreFlightOrigin("");
    const auto wx = r.planner.wxCalls.size();
    const int comms = r.planner.commsCalls;
    r.wait(100);
    EXPECT_EQ(r.planner.wxCalls.size(), wx);
    EXPECT_EQ(r.planner.commsCalls, comms);
}

TEST(FlightWatcher, PreFlightHandsOverAtBlockOut) {
    WatcherRig r;
    r.bridge.applyConnection(blocktime::ConnectionState::Connected);
    r.watcher->setPreFlightOrigin("KBFI");
    ASSERT_EQ(r.pushed("atis").size(), 1u);
    r.blockOut();
    // Block out logs the history and the ATIS again, with the flight id (the debrief keeps everything).
    const auto atis = r.pushed("atis");
    ASSERT_EQ(atis.size(), 2u);
    EXPECT_EQ(atis[1].data["flight_id"], "2026-11-23T22:51:08.969Z");
    const auto exchanges = r.pushed("atc_exchange");
    EXPECT_EQ(exchanges.back().data["flight_id"], "2026-11-23T22:51:08.969Z");
}

TEST(FlightWatcher, NoPreFlightAfterBlockIn) {
    WatcherRig r;
    r.bridge.applyConnection(blocktime::ConnectionState::Connected);
    r.watcher->setPreFlightOrigin("KBFI");
    r.blockOut();
    blocktime::BlockIn in;
    in.flightId = "2026-11-23T22:51:08.969Z";
    r.bridge.applyBlockEvent(in);
    const auto wx = r.planner.wxCalls.size();
    const int comms = r.planner.commsCalls;
    r.wait(100);
    EXPECT_EQ(r.planner.wxCalls.size(), wx);
    EXPECT_EQ(r.planner.commsCalls, comms);
    r.watcher->setPreFlightOrigin("");
    r.watcher->setPreFlightOrigin("KPDX");  // the next leg, planned at the gate
    EXPECT_EQ(r.planner.wxCalls.back(), "KPDX");
}

// Review finding: a turnaround's origin is the last leg's destination, whose ATIS letter is often unchanged since the
// descent; the gate must log it again, or the panel stays without wind, OAT and altimeter.
TEST(FlightWatcher, TurnaroundLogsTheNextOriginsAtisAgain) {
    WatcherRig r;
    r.bridge.applyConnection(blocktime::ConnectionState::Connected);
    r.blockOut();
    r.planner.wxBody = R"({"airports":[{"airport":"KPDX","current":"B","atis":"Portland information Bravo"}]})";
    r.flight("phase_changed", {{"phase", "descent"}});
    blocktime::BlockIn in;
    in.flightId = "2026-11-23T22:51:08.969Z";
    r.bridge.applyBlockEvent(in);
    const auto before = r.pushed("atis").size();
    r.watcher->setPreFlightOrigin("KPDX");  // the next leg, from the same airport
    const auto atis = r.pushed("atis");
    ASSERT_EQ(atis.size(), before + 1);
    EXPECT_EQ(atis.back().data["role"], "origin");
    EXPECT_EQ(atis.back().data["flight_id"], "");
}

// Review finding: after an interrupt the flight is still planned and the clearance still valid; the gate logs the
// history again so the panel (which starts over at the interrupt) gets the clearance back.
TEST(FlightWatcher, InterruptLogsTheHistoryAgainAtTheGate) {
    WatcherRig r;
    r.bridge.applyConnection(blocktime::ConnectionState::Connected);
    r.watcher->setPreFlightOrigin("KBFI");
    r.blockOut();
    const auto before = r.pushed("atc_exchange").size();
    blocktime::FlightInterrupted interrupted;
    interrupted.flightId = "2026-11-23T22:51:08.969Z";
    r.bridge.applyBlockEvent(interrupted);
    const auto exchanges = r.pushed("atc_exchange");
    ASSERT_GT(exchanges.size(), before);
    EXPECT_EQ(exchanges.back().data["flight_id"], "");
}

TEST(FlightWatcher, AtcEveryPollAndOnceAtBlockIn) {
    WatcherRig r;
    r.blockOut();
    r.wait(100);
    EXPECT_GT(r.planner.commsCalls, 1);
    const auto exchanges = r.pushed("atc_exchange");
    ASSERT_EQ(exchanges.size(), 2u);  // each exchange once, however many polls
    EXPECT_EQ(exchanges[0].data["si_flight_id"], "987654321");
    EXPECT_EQ(exchanges[0].data["entry"]["station_name"], "Boeing Ground");
    blocktime::BlockIn in;
    in.flightId = "2026-11-23T22:51:08.969Z";
    const int before = r.planner.commsCalls;
    r.bridge.applyBlockEvent(in);
    EXPECT_EQ(r.planner.commsCalls, before + 1);
    r.wait(100);
    EXPECT_EQ(r.planner.commsCalls, before + 1);
}

TEST(FlightWatcher, FailedCallsLogNothingAndRetry) {
    WatcherRig r;
    r.planner.wxError = "SayIntentions didn't answer";
    r.planner.commsError = "SayIntentions: No active flight could be found.";
    r.blockOut();
    r.wait(60);
    EXPECT_TRUE(r.pushed("atis").empty());
    EXPECT_TRUE(r.pushed("atc_exchange").empty());
    r.planner.wxError.clear();
    r.planner.commsError.clear();
    r.wait(100);
    EXPECT_EQ(r.pushed("atis").size(), 1u);
    EXPECT_EQ(r.pushed("atc_exchange").size(), 2u);
}

TEST(FlightWatcher, InterruptStopsWithoutALastFetch) {
    WatcherRig r;
    r.blockOut();
    const int before = r.planner.commsCalls;
    blocktime::FlightInterrupted e;
    e.flightId = "2026-11-23T22:51:08.969Z";
    e.reason = "sim_stopped";
    r.bridge.applyBlockEvent(e);
    EXPECT_EQ(r.planner.commsCalls, before);
    const auto wx = r.planner.wxCalls.size();
    r.wait(100);
    EXPECT_EQ(r.planner.commsCalls, before);
    EXPECT_EQ(r.planner.wxCalls.size(), wx);
}

// --- RecorderBridge: the six stages (D-128) -----------------------------------------------------------------------

namespace {

QVariantMap phaseEvent(const QString& phase, const QString& hms) {
    return {{"phase", phase}, {"time", "2026-11-23T" + hms + ".000Z"}, {"alt_ft", 25}, {"flight_level", false}};
}

QVariantMap stage(RecorderBridge& b, int i) { return b.stages().at(i).toMap(); }

}  // namespace

TEST(RecorderBridge, StagesFollowFlightEvents) {
    qtApp();
    RecorderBridge b;
    EXPECT_EQ(b.stages().size(), 6);
    EXPECT_EQ(stage(b, 0)["state"].toString(), "current");
    int changes = 0;
    QObject::connect(&b, &RecorderBridge::stagesChanged, [&changes] { ++changes; });
    b.applyFlightEvent("phase_changed", phaseEvent("pushback", "22:51:08"));
    EXPECT_GE(changes, 1);
    const QVariantMap departure = stage(b, 1);
    EXPECT_EQ(departure["name"].toString(), "Departure");
    EXPECT_EQ(departure["state"].toString(), "current");
    const QVariantList lines = departure["lines"].toList();
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0].toMap()["text"].toString(), "Pushback");
    EXPECT_EQ(lines[0].toMap()["start"].toString(), "22:51Z");
    EXPECT_TRUE(lines[0].toMap()["running"].toBool());
    EXPECT_EQ(b.phase(), "Pushback");
}

TEST(RecorderBridge, StagesResetAtBlockOut) {
    qtApp();
    RecorderBridge b;
    b.applyFlightEvent("phase_changed", phaseEvent("pushback", "22:51:08"));
    b.applyFlightEvent("phase_changed", phaseEvent("taxi_out", "22:54:17"));
    blocktime::BlockOut out;
    out.flightId = "2026-11-24T10:00:00.000Z";
    b.applyBlockEvent(out);
    EXPECT_TRUE(stage(b, 1)["lines"].toList().isEmpty());
    EXPECT_EQ(b.phase(), "");
}

TEST(RecorderBridge, StagesResetOnInterrupt) {
    qtApp();
    RecorderBridge b;
    b.applyFlightEvent("phase_changed", phaseEvent("pushback", "22:51:08"));
    blocktime::FlightInterrupted e;
    e.flightId = "2026-11-23T22:51:08.969Z";
    e.reason = "sim_stopped";
    b.applyBlockEvent(e);
    EXPECT_TRUE(stage(b, 1)["lines"].toList().isEmpty());
    EXPECT_EQ(stage(b, 0)["state"].toString(), "current");
}

// --- RecorderBridge: GSX and the progress card across the gate (milestone 8, D-134, D-135) ------------------------

TEST(RecorderBridge, GsxServicesBeforeBlockOut) {
    qtApp();
    RecorderBridge b;
    EXPECT_FALSE(b.hasNextLegServices());
    b.applyFlightEvent("gsx_service",
                       {{"service", "refuelling"}, {"state", "performing"}, {"time", "2026-11-24T09:30:00.000Z"}});
    EXPECT_TRUE(b.hasNextLegServices());
    EXPECT_FALSE(b.showingFlight());
    const QVariantList lines = stage(b, 0)["lines"].toList();
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0].toMap()["text"].toString(), "Refueling");
    EXPECT_TRUE(lines[0].toMap()["checks"].toList().isEmpty());
}

TEST(RecorderBridge, PillMarkerFollowsBoardingAndTouchdown) {
    qtApp();
    RecorderBridge b;
    EXPECT_EQ(b.pillMarker()["label"].toString(), "Preparing");
    b.applyFlightEvent("gsx_service",
                       {{"service", "boarding"}, {"state", "performing"}, {"time", "2026-11-23T09:30:00.000Z"}});
    EXPECT_EQ(b.pillMarker()["section"].toInt(), 0);
    EXPECT_EQ(b.pillMarker()["label"].toString(), "Boarding");
    b.applyFlightEvent("phase_changed", phaseEvent("approach", "11:00:00"));
    EXPECT_EQ(b.pillMarker()["section"].toInt(), 4);
    b.applyFlightEvent("phase_changed", phaseEvent("landing", "11:10:00"));  // Arrival from touchdown (D-184)
    EXPECT_EQ(b.pillMarker()["section"].toInt(), 5);
    EXPECT_EQ(b.pillMarker()["part"].toInt(), 0);
    EXPECT_EQ(b.pillMarker()["label"].toString(), "Landing");
    EXPECT_EQ(b.pill()[4].toMap()["state"].toString(), "done");
}

TEST(RecorderBridge, ArrivalStaysUntilShowNextLeg) {
    qtApp();
    RecorderBridge b;
    BlockOut out;
    out.flightId = "2026-11-24T10:00:00.000Z";
    out.time = std::chrono::sys_days{std::chrono::November / 24 / 2026} + std::chrono::hours(10);
    b.applyBlockEvent(out);
    b.applyFlightEvent("phase_changed", phaseEvent("pushback", "10:00:00"));
    BlockIn in;
    in.flightId = out.flightId;
    in.time = out.time + std::chrono::hours(2);
    b.applyBlockEvent(in);
    b.applyFlightEvent("phase_changed", phaseEvent("arrived", "12:00:00"));
    EXPECT_TRUE(b.showingFlight());

    TelemetrySample s{};
    s.parkingBrake = 1;
    s.onGround = 1;
    s.numEngines = 2;
    s.lightBeacon = 0;
    s.lightStrobe = 0;
    b.applySample(s);
    const QVariantList arrival = stage(b, 5)["lines"].toList();
    ASSERT_EQ(arrival.size(), 2);
    EXPECT_EQ(arrival[1].toMap()["text"].toString(), "Ready for shutdown or turnaround");
    const QVariantList checks = arrival[1].toMap()["checks"].toList();
    ASSERT_EQ(checks.size(), 4);
    EXPECT_EQ(checks[0].toMap()["text"].toString(), "Engines off");
    EXPECT_TRUE(checks[0].toMap()["done"].toBool());

    b.showNextLeg();
    EXPECT_FALSE(b.showingFlight());
    EXPECT_TRUE(stage(b, 5)["lines"].toList().isEmpty());
}

// D-166: Live's times come from the bridge's clock, so a replay shows the flight's own times.
TEST(RecorderBridge, FeedUsesTheBridgeClock) {
    qtApp();
    RecorderBridge b;
    const TimePoint fixed = *parseUtc("2026-11-24T23:53:16.000Z");
    b.setClock([fixed] { return fixed; });
    EXPECT_EQ(b.now(), fixed);
    EXPECT_DOUBLE_EQ(b.nowMs(), 1795564396000.0);
    b.applyLine("x", false);
    EXPECT_EQ(b.events()->data(b.events()->index(0), EventFeedModel::TimeRole).toString(), "23:53:16");
}

TEST(RecorderBridge, ShutdownChecksUseTheBridgeClock) {
    qtApp();
    RecorderBridge b;
    b.setClock([] { return *parseUtc("2026-11-24T12:34:56.000Z"); });
    BlockOut out;
    out.flightId = "2026-11-24T10:00:00.000Z";
    out.time = std::chrono::sys_days{std::chrono::November / 24 / 2026} + std::chrono::hours(10);
    b.applyBlockEvent(out);
    BlockIn in;
    in.flightId = out.flightId;
    in.time = out.time + std::chrono::hours(2);
    b.applyBlockEvent(in);
    b.applyFlightEvent("phase_changed", phaseEvent("arrived", "12:00:00"));
    TelemetrySample s{};
    s.parkingBrake = 1;
    s.onGround = 1;
    s.numEngines = 2;
    b.applySample(s);
    const QVariantList arrival = stage(b, 5)["lines"].toList();
    ASSERT_EQ(arrival.size(), 2);
    EXPECT_EQ(arrival[1].toMap()["start"].toString(), "12:34Z")
        << arrival[1].toMap()["start"].toString().toStdString();
}

TEST(RecorderBridge, ReplayConnection) {
    qtApp();
    RecorderBridge b;
    b.setReplay();
    EXPECT_EQ(b.connection(), "replay");
}

TEST(RecorderBridge, BeaconOnKeepsReadyAway) {
    qtApp();
    RecorderBridge b;
    BlockOut out;
    out.flightId = "2026-11-24T10:00:00.000Z";
    b.applyBlockEvent(out);
    BlockIn in;
    in.flightId = out.flightId;
    b.applyBlockEvent(in);
    b.applyFlightEvent("phase_changed", phaseEvent("arrived", "12:00:00"));
    TelemetrySample s{};
    s.parkingBrake = 1;
    s.lightBeacon = 1;
    b.applySample(s);
    EXPECT_EQ(stage(b, 5)["lines"].toList().size(), 1);  // Parked only
}

// D-200: the destination ATIS from 200 nm out, for the cruise panel's DEST ATIS and the arrival readouts.
TEST(FlightWatcher, DestinationAtisFrom200Nm) {
    WatcherRig r;
    r.blockOut();
    r.flight("liftoff");
    r.planner.wxCalls.clear();
    r.planner.wxBody = R"({"airports":[{"airport":"KPDX","current":"B","atis":"Portland information Bravo"}]})";
    r.flight("flight_marker", {{"marker", "within_200nm"}});
    ASSERT_EQ(r.planner.wxCalls.size(), 1);
    EXPECT_EQ(r.planner.wxCalls.front(), "KPDX");
    const auto atis = r.pushed("atis");
    ASSERT_EQ(atis.size(), 2u);
    EXPECT_EQ(atis[1].data["role"], "destination");
    r.planner.wxCalls.clear();
    r.flight("phase_changed", {{"phase", "descent"}});  // already polling: no extra call
    EXPECT_TRUE(r.planner.wxCalls.isEmpty());
}

TEST(FlightWatcher, DestinationStartsAtApproachWithoutADescent) {
    WatcherRig r;
    r.blockOut();
    r.flight("liftoff");
    r.planner.wxCalls.clear();
    r.flight("phase_changed", {{"phase", "approach"}});  // Cruise straight to Approach (D-124's fallback)
    ASSERT_FALSE(r.planner.wxCalls.isEmpty());
    EXPECT_EQ(r.planner.wxCalls.front(), "KPDX");
    r.planner.wxCalls.clear();
    r.flight("phase_changed", {{"phase", "approach"}});  // after a go-around: already polling, no extra call
    EXPECT_TRUE(r.planner.wxCalls.isEmpty());
}

TEST(FlightWatcher, SendsTheGateAndLogsOnce) {
    WatcherRig r;
    r.loadDestination();
    r.sayIntentionsPicksE9();
    r.blockOut();
    r.wait(100);
    EXPECT_EQ(r.planner.assignCalls, QStringList{"E7@KPDX"});
    EXPECT_EQ(r.planner.parkingCalls, 1);
    const auto gates = r.pushed("arrival_gate");
    ASSERT_EQ(gates.size(), 1u);
    EXPECT_EQ(gates[0].data["result"], "assigned");
    EXPECT_EQ(gates[0].data["gate"], "Gate E7");
    EXPECT_EQ(gates[0].data["si_gate"], "Gate E9");
    EXPECT_EQ(gates[0].data["si_parking"], "Gate E7");
    EXPECT_EQ(gates[0].data["flight_id"], "2026-11-23T22:51:08.969Z");
    EXPECT_TRUE(gates[0].data.contains("time"));
}

TEST(FlightWatcher, KeepsSayIntentionsGateWithoutAPreference) {
    WatcherRig r;
    r.preferred.clear();
    r.loadDestination();
    r.sayIntentionsPicksE9();
    r.blockOut();
    r.wait(100);
    EXPECT_TRUE(r.planner.assignCalls.isEmpty());
    const auto gates = r.pushed("arrival_gate");
    ASSERT_EQ(gates.size(), 1u);
    EXPECT_EQ(gates[0].data["result"], "kept");
    EXPECT_EQ(gates[0].data["gate"], "Gate E9");
}

TEST(FlightWatcher, NoSayIntentionsNoGate) {
    WatcherRig r;
    r.loadDestination();
    r.blockOut();
    r.wait(100);
    EXPECT_GT(r.planner.siFlightCalls, 1);  // keeps asking quietly
    EXPECT_TRUE(r.planner.assignCalls.isEmpty());
    EXPECT_TRUE(r.pushed("arrival_gate").empty());
}

TEST(FlightWatcher, GateWaitsForTheAirport) {
    WatcherRig r;
    r.sayIntentionsPicksE9();
    r.blockOut();
    r.wait(80);
    EXPECT_TRUE(r.planner.assignCalls.isEmpty());
    r.loadDestination();
    r.wait(80);
    EXPECT_EQ(r.planner.assignCalls, QStringList{"E7@KPDX"});
}

TEST(FlightWatcher, UnknownGateLogged) {
    WatcherRig r;
    r.loadDestination();
    r.sayIntentionsPicksE9();
    r.planner.assignError = QStringLiteral("SayIntentions: Unknown gate: E7");
    r.blockOut();
    r.wait(100);
    EXPECT_EQ(r.planner.assignCalls.size(), 1);
    EXPECT_EQ(r.planner.parkingCalls, 0);
    const auto gates = r.pushed("arrival_gate");
    ASSERT_EQ(gates.size(), 1u);
    EXPECT_EQ(gates[0].data["result"], "unknown_gate");
}

TEST(FlightWatcher, GateStopsAtBlockIn) {
    WatcherRig r;
    r.loadDestination();
    r.blockOut();
    blocktime::BlockIn in;
    in.flightId = "2026-11-23T22:51:08.969Z";
    r.bridge.applyBlockEvent(in);
    const int calls = r.planner.siFlightCalls;
    r.sayIntentionsPicksE9();
    r.wait(100);
    EXPECT_EQ(r.planner.siFlightCalls, calls);
    EXPECT_TRUE(r.planner.assignCalls.isEmpty());
}

TEST(YourFlightModel, LoadInTextNamesTheSpot) {
    db::Database db = deliveredAirline();
    ASSERT_TRUE(db.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb, arrival_parking)"
        " SELECT 'x', a.airplane_id, '8', 'vip', o.airport_id, o.airport_id, o.airport_id, 0, 4100,"
        " '2026-11-20T10:00:00Z', '2026-11-20T11:00:00Z', '2026-11-20T12:00:00Z', 0, 9000, 8000, 'Parking 70'"
        " FROM airplane a, airport o WHERE a.registration = 'N814SX' AND o.icao_code = 'KBFI'"));
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    YourFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    EXPECT_EQ(m.loadInText(), "Load in at KBFI, Boeing Field, Parking 70");
}

TEST_F(LiveFlight, SummaryNamesTheParkedSpot) {
    write({liveOut(plannedId_),
           liveLine("block_in", std::string(R"({"flight_id":")") + kLiveOut + R"(","time":")" + kLiveIn +
                                    R"(","lat":32.7336,"lon":-117.19,"shutdown_fuel_lb":9400,"block_time_s":10236,)"
                                    R"("parking":"Gate 20"})")});
    auto m = model();
    m->checkFlown();
    FlightListModel list(db_, dir_, "XLY");
    list.reloadBlocking();
    ASSERT_EQ(list.count(), 1);
    EXPECT_EQ(list.summary(0)["parkedText"].toString(), "Parked at Gate 20");
}

TEST(RecorderBridge, EnginesRunningPerEngine) {
    qtApp();
    RecorderBridge b;
    blocktime::TelemetrySample s{};
    s.numEngines = 2;
    s.engCombustion[1] = 1;
    s.mach = 0.78;
    b.applySample(s);
    EXPECT_EQ(b.engineRunning(), (QVariantList{false, true}));
    EXPECT_TRUE(b.showMach());
    EXPECT_EQ(b.machText(), "M.78");
    s.mach = 0.68;  // between the lines: stays Mach
    b.applySample(s);
    EXPECT_TRUE(b.showMach());
    s.mach = 0.6;
    b.applySample(s);
    EXPECT_FALSE(b.showMach());
}

// --- ReviewWriter: Claude's review, once after settling (D-161) ---------------------------------------------------

namespace {

constexpr const char* kDebriefFlight = "2026-11-24T23:53:16.837Z";
constexpr const char* kTestKey = "sk-ant-test-123";

class FakeReviewClient : public ReviewClient {
public:
    bool hasKey() const override { return key; }
    void write(std::string body, std::function<void(int, QByteArray, QString)> done) override {
        bodies.push_back(std::move(body));
        pending = std::move(done);
    }
    void answer(int status, const std::string& body, const QString& networkError = {}) {
        auto done = std::move(pending);
        pending = nullptr;
        done(status, QByteArray::fromStdString(body), networkError);
    }

    bool key = true;
    std::vector<std::string> bodies;
    std::function<void(int, QByteArray, QString)> pending;
};

const char* kGoodAnswer = R"({"model":"claude-opus-5-5","stop_reason":"end_turn","content":[{"type":"text",
    "text":"{\"summary\":\"A tidy flight.\",\"work_on\":[\"Beacon first\"],\"landing\":\"Firm.\",\"atc\":\"\"}"}]})";

// The KLAS-KSAN fixture settled as the planned flight: a measured flight with no review.
class Debrief : public LiveFlight {
protected:
    void SetUp() override {
        LiveFlight::SetUp();
        std::ifstream in(BLOCKTIME_SOURCE_DIR "/tests/data/flight-klas-ksan/events-2026-11-24.jsonl", std::ios::binary);
        std::ofstream out(dir_ / "events-2026-11-24.jsonl", std::ios::binary);
        const std::string from = "\"planned_flight_id\":1,";
        std::string l;
        while (std::getline(in, l)) {
            const auto at = l.find(from);
            if (at != std::string::npos) {
                l.replace(at, from.size(), "\"planned_flight_id\":" + std::to_string(plannedId_) + ",");
            }
            out << l << "\n";
        }
        out.close();
        std::string error;
        ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    }

    std::int64_t reviews() {
        db::Statement q(db_, "SELECT COUNT(*) FROM flight_review");
        return q.next() ? q.integer(0) : -1;
    }

    FakeReviewClient client_;
};

}  // namespace

TEST_F(Debrief, WritesOnceAfterSettling) {
    ReviewWriter w(db_, dir_, client_);
    EXPECT_EQ(w.state(kDebriefFlight), "missing");
    int changes = 0;
    QObject::connect(&w, &ReviewWriter::reviewChanged, [&](const QString& id) {
        EXPECT_EQ(id, kDebriefFlight);
        ++changes;
    });
    w.onSettled(kDebriefFlight);
    ASSERT_EQ(client_.bodies.size(), 1u);
    EXPECT_EQ(w.state(kDebriefFlight), "writing");
    const auto body = nlohmann::json::parse(client_.bodies[0]);
    EXPECT_NE(body["messages"][0]["content"].get<std::string>().find("KBFI → KSAN"), std::string::npos);
    client_.answer(200, kGoodAnswer);
    EXPECT_EQ(w.state(kDebriefFlight), "written");
    EXPECT_EQ(reviews(), 1);
    EXPECT_EQ(loadReviews(db_).at(kDebriefFlight).workOn, std::vector<std::string>{"Beacon first"});
    EXPECT_EQ(changes, 2);  // writing, then written
    w.onSettled(kDebriefFlight);  // already written: no second call
    EXPECT_EQ(client_.bodies.size(), 1u);
}

// Review 5.1: at block in the review gets the lines settlement already read, so it doesn't read the log again.
TEST_F(Debrief, UsesTheLinesSettlementRead) {
    const auto lines = loadFlightEvents(dir_, kDebriefFlight);
    ASSERT_FALSE(lines.empty());
    std::filesystem::remove(dir_ / "events-2026-11-24.jsonl");
    ReviewWriter w(db_, dir_, client_);
    w.onSettled(kDebriefFlight, lines);
    ASSERT_EQ(client_.bodies.size(), 1u);
    w.retry(kDebriefFlight);  // Retry, or a restart, reads the log: gone here
    client_.answer(529, "{}");
    w.retry(kDebriefFlight);
    EXPECT_EQ(client_.bodies.size(), 1u);
    EXPECT_EQ(w.error(kDebriefFlight), "The flight's event log is gone");
}

TEST_F(Debrief, FailureStoresNothingAndKeepsReason) {
    ReviewWriter w(db_, dir_, client_);
    w.onSettled(kDebriefFlight);
    client_.answer(529, R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})");
    EXPECT_EQ(w.state(kDebriefFlight), "failed");
    EXPECT_EQ(w.error(kDebriefFlight), "Claude: Overloaded");
    EXPECT_EQ(w.error(kDebriefFlight).indexOf(kTestKey), -1);
    EXPECT_EQ(reviews(), 0);
}

TEST_F(Debrief, NetworkErrorSaysOnlyThat) {
    ReviewWriter w(db_, dir_, client_);
    w.onSettled(kDebriefFlight);
    client_.answer(0, "", "Host api.anthropic.com not found");
    EXPECT_EQ(w.error(kDebriefFlight), "Couldn't reach Claude");
}

TEST_F(Debrief, RetryCallsAgain) {
    ReviewWriter w(db_, dir_, client_);
    w.onSettled(kDebriefFlight);
    client_.answer(529, "{}");
    w.retry(kDebriefFlight);
    ASSERT_EQ(client_.bodies.size(), 2u);
    w.retry(kDebriefFlight);  // already writing: ignored
    EXPECT_EQ(client_.bodies.size(), 2u);
    client_.answer(200, kGoodAnswer);
    EXPECT_EQ(w.state(kDebriefFlight), "written");
    EXPECT_EQ(w.error(kDebriefFlight), "");
}

TEST_F(Debrief, NoAutomaticCallForOldFlights) {
    ReviewWriter w(db_, dir_, client_);  // e.g. after a restart: a measured flight with no review
    EXPECT_EQ(w.state(kDebriefFlight), "missing");
    EXPECT_TRUE(client_.bodies.empty());
}

TEST_F(Debrief, NoKeyMakesNoCall) {
    client_.key = false;
    ReviewWriter w(db_, dir_, client_);
    w.onSettled(kDebriefFlight);
    EXPECT_TRUE(client_.bodies.empty());
    EXPECT_EQ(w.state(kDebriefFlight), "no_key");
}

TEST_F(Debrief, UnmeasuredFlightHasNoState) {
    ReviewWriter w(db_, dir_, client_);
    EXPECT_EQ(w.state("2020-01-01T00:00:00.000Z"), "none");
    w.retry("2020-01-01T00:00:00.000Z");
    EXPECT_TRUE(client_.bodies.empty());
}

// --- The debrief in the Flight Summary and the Flights strip (Task 9) ---------------------------------------------

TEST_F(Debrief, SummaryHasDebrief) {
    FlightListModel list(db_, dir_, "XLY");
    list.reloadBlocking();
    const int row = list.rowOf(kDebriefFlight);
    ASSERT_GE(row, 0);
    const QVariantMap d = list.summary(row)["debrief"].toMap();
    EXPECT_TRUE(d["measured"].toBool());
    EXPECT_EQ(d["scoreText"].toString(), "62");
    EXPECT_EQ(d["starsText"].toString(), QString::fromUtf8("★★★☆☆"));
    EXPECT_EQ(d["landingPointsText"].toString(), QString::fromUtf8("−8"));
    const QVariantList checks = d["checks"].toList();
    ASSERT_EQ(checks.size(), 14);
    int missed = 0;
    for (const QVariant& c : checks) missed += c.toMap()["state"].toString() == "missed";
    EXPECT_EQ(missed, 5);
    const QVariantMap beacon = checks[0].toMap();
    EXPECT_EQ(beacon["label"].toString(), "Beacon on before pushback");
    EXPECT_EQ(beacon["detail"].toString(), "2 min 43 s late");
    EXPECT_EQ(beacon["pointsText"].toString(), QString::fromUtf8("−6"));
    EXPECT_EQ(checks[2].toMap()["pointsText"].toString(), "");
    const QStringList landing = d["landingLines"].toStringList();
    ASSERT_EQ(landing.size(), 2);
    EXPECT_TRUE(landing[0].startsWith(QString::fromUtf8("591 fpm · 1.47 G · no bounce · VREF +0 · wind 330/9"))) << landing[0].toStdString();
    EXPECT_EQ(landing[1], QString::fromUtf8("2.7 m off the centerline, 0.6° off the heading (information)"));
    EXPECT_EQ(d["reviewState"].toString(), "missing");
}

TEST_F(Debrief, AverageStripText) {
    FlightListModel list(db_, dir_, "XLY");
    list.reloadBlocking();
    EXPECT_EQ(list.averageScoreText(), "Average score 62 (last 10: 62)");
    EXPECT_EQ(list.averageStarsText(), QString::fromUtf8("Landings ★★★☆☆ 3.0 (last 10: 3.0)"));
}

TEST_F(Debrief, OldFlightHasNoDebrief) {
    ASSERT_TRUE(db_.exec("DELETE FROM flight_debrief"));  // as a flight settled before 0.11.0
    FlightListModel list(db_, dir_, "XLY");
    list.reloadBlocking();
    const QVariantMap d = list.summary(list.rowOf(kDebriefFlight))["debrief"].toMap();
    EXPECT_FALSE(d["measured"].toBool());
    EXPECT_EQ(list.averageScoreText(), "");
    EXPECT_EQ(list.averageStarsText(), "");
}

TEST_F(Debrief, SummaryShowsTheWrittenReview) {
    ReviewWriter w(db_, dir_, client_);
    FlightListModel list(db_, dir_, "XLY", &w);
    w.onSettled(kDebriefFlight);
    list.reloadBlocking();
    EXPECT_EQ(list.summary(list.rowOf(kDebriefFlight))["debrief"].toMap()["reviewState"].toString(), "writing");
    client_.answer(200, kGoodAnswer);
    list.reloadBlocking();
    const QVariantMap d = list.summary(list.rowOf(kDebriefFlight))["debrief"].toMap();
    EXPECT_EQ(d["reviewState"].toString(), "written");
    const QVariantMap r = d["review"].toMap();
    EXPECT_EQ(r["summary"].toString(), "A tidy flight.");
    EXPECT_EQ(r["workOn"].toStringList(), QStringList{"Beacon first"});
}

TEST_F(Debrief, RetryFindsALogOlderThanAWeek) {
    // Logs are kept 30 days: a flight with eight newer daily logs can still be reviewed.
    for (int day = 25; day <= 30; ++day) std::ofstream(dir_ / std::format("events-2026-11-{}.jsonl", day));
    for (int day = 1; day <= 2; ++day) std::ofstream(dir_ / std::format("events-2026-12-0{}.jsonl", day));
    ReviewWriter w(db_, dir_, client_);
    w.retry(kDebriefFlight);
    EXPECT_EQ(client_.bodies.size(), 1u);
    EXPECT_EQ(w.error(kDebriefFlight), "");
}

// The replay's planned flight (D-175): built from the settled flight, it follows the replay bridge and writes nothing.
namespace {

PlannedFlightView replayView() {
    PlannedFlightView v;
    v.registration = "N814SX";
    v.typeCode = "B737-BBJ2";
    v.origin = "KLAS";
    v.originName = "Harry Reid Intl";
    v.destination = "KSAN";
    v.destinationName = "San Diego Intl";
    v.flightNumber = "1231";
    v.kind = FlightKind::Vip;
    v.cabins = {{"economy", 40}};
    v.fuelPriceCentsPer100Lb = 4100;
    v.plannedAt = "2026-11-24T22:00:00Z";
    return v;
}

}  // namespace

TEST(ReplayPlannedFlight, FollowsTheBridge) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(replayView(), "XLY", Rates{}, b, "2026-11-24T23:53:16.837Z");
    EXPECT_TRUE(f.hasFlight());
    EXPECT_EQ(f.state(), "replay");
    EXPECT_EQ(f.stateText(), "Replay");
    EXPECT_EQ(f.title(), QString::fromUtf8("XLY1231 · KLAS → KSAN"));
    EXPECT_EQ(f.airplaneText(), QString::fromUtf8("N814SX · B737-BBJ2"));
    EXPECT_EQ(f.notRecording(), "");

    BlockOut out;
    out.flightId = "2026-11-24T23:53:16.837Z";
    out.time = *parseUtc(out.flightId);
    b.applyBlockEvent(out);
    EXPECT_EQ(f.state(), "flying");

    BlockIn in;
    in.flightId = out.flightId;
    in.time = out.time + std::chrono::hours(1);
    b.applyBlockEvent(in);
    EXPECT_EQ(f.state(), "summary");
    EXPECT_FALSE(f.hasFlight());
    EXPECT_EQ(f.summaryFlightId(), "2026-11-24T23:53:16.837Z");
}

// The replay's stages drive the same panels as Your Flight: Pre-Flight before block out, the in-flight panel after.
TEST(ReplayPlannedFlight, StagesDriveThePanels) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(replayView(), "XLY", Rates{}, b, "2026-11-24T23:53:16.837Z");
    DashboardHeader h(b, f, "XLY", "Soxxly Airlines", DashboardHeader::SettledFlight{});
    PreFlightModel pre(b, f);
    InFlightModel in(b, f, 3, &h);
    EXPECT_EQ(f.stage(), YourFlightStage::Accepted);
    EXPECT_TRUE(pre.visible());
    EXPECT_FALSE(in.visible());
    BlockOut out;
    out.flightId = "2026-11-24T23:53:16.837Z";
    out.time = *parseUtc(out.flightId);
    b.applyBlockEvent(out);
    EXPECT_EQ(f.stage(), YourFlightStage::Flying);
    EXPECT_FALSE(pre.visible());
    EXPECT_TRUE(in.visible());
    BlockIn bin;
    bin.flightId = out.flightId;
    bin.time = out.time + std::chrono::hours(1);
    b.applyBlockEvent(bin);
    EXPECT_EQ(f.stage(), YourFlightStage::Arrived);  // the arrival view until Settle Up
    EXPECT_TRUE(in.visible());
    in.finishFlight();
    EXPECT_EQ(f.stage(), YourFlightStage::Summary);
    EXPECT_FALSE(in.visible());
}

// A replay without its settled flight (an older database) shows no Pre-Flight.
TEST(ReplayPlannedFlight, NoViewNoPreFlight) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(std::nullopt, "XLY", Rates{}, b, "2026-11-24T23:53:16.837Z");
    PreFlightModel pre(b, f);
    EXPECT_EQ(f.stage(), YourFlightStage::Empty);
    EXPECT_FALSE(pre.visible());
    BlockOut out;
    out.flightId = "2026-11-24T23:53:16.837Z";
    out.time = *parseUtc(out.flightId);
    b.applyBlockEvent(out);
    EXPECT_EQ(f.stage(), YourFlightStage::Flying);
    EXPECT_FALSE(pre.visible());
}

TEST(ReplayPlannedFlight, IsReadOnly) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(replayView(), "XLY", Rates{}, b, "2026-11-24T23:53:16.837Z");
    EXPECT_TRUE(f.readOnly());
    f.cancelFlight();
    f.importPlan();
    f.doneForNow();
    EXPECT_TRUE(f.hasFlight());
    EXPECT_EQ(f.message(), "");
    EXPECT_FALSE(f.busy());
}

// ReplaySession (D-175): Dave's KSAN → KSJC leg of 2026-11-25 played into a fresh bridge.
namespace {

const fs::path kReplayFixtureDir = BLOCKTIME_SOURCE_DIR "/tests/data/flight-ksan-ksjc";
const QString kReplayFlight = "2026-11-25T18:43:04.053Z";

bool feedHas(RecorderBridge& b, const QString& start) {
    for (int i = 0; i < b.events()->rowCount(); ++i) {
        if (feedText(b, i).startsWith(start)) return true;
    }
    return false;
}

}  // namespace

TEST(ReplaySession, PlaysToBlockIn) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, std::nullopt, "XLY", Rates{});
    ASSERT_EQ(r.error(), "");
    EXPECT_EQ(r.title(), QString::fromUtf8("N814SX · 25 Nov"));
    EXPECT_EQ(r.bridge()->connection(), "replay");
    ASSERT_TRUE(r.seek("0:00"));
    EXPECT_TRUE(r.bridge()->flightOpen());
    r.setSpeed(10);
    for (int i = 0; i < 100 && !r.finished(); ++i) r.tick(std::chrono::minutes(1));
    ASSERT_TRUE(r.finished());
    RecorderBridge& b = *r.bridge();
    EXPECT_FALSE(b.flightOpen());
    EXPECT_TRUE(b.lastFlightSummary().startsWith("Block in 1:18")) << b.lastFlightSummary().toStdString();
    EXPECT_TRUE(feedHas(b, "BLOCK OUT"));
    EXPECT_TRUE(feedHas(b, "TOUCHDOWN"));
    EXPECT_TRUE(feedHas(b, "BLOCK IN"));
    EXPECT_EQ(r.plannedFlight()->state(), "summary");
    EXPECT_EQ(r.plannedFlight()->summaryFlightId(), kReplayFlight);
}

TEST(ReplaySession, FeedShowsTheFlightsOwnTimes) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, std::nullopt, "XLY", Rates{});
    ASSERT_TRUE(r.seek("block_out"));
    RecorderBridge& b = *r.bridge();
    int row = -1;
    for (int i = 0; i < b.events()->rowCount(); ++i) {
        if (feedText(b, i).startsWith("BLOCK OUT")) row = i;
    }
    ASSERT_GE(row, 0);
    // The block-out line was logged 18:43:15, eleven seconds after the release it reports.
    EXPECT_EQ(b.events()->data(b.events()->index(row), EventFeedModel::TimeRole).toString(), "18:43:15");
    // A GSX line from the lead-in keeps its own time, though the seek fed it at once.
    EXPECT_NE(b.events()->data(b.events()->index(b.events()->rowCount() - 1), EventFeedModel::TimeRole).toString(),
              "18:43:15");
}

TEST(ReplaySession, SeekToCruise) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, std::nullopt, "XLY", Rates{});
    ASSERT_TRUE(r.seek("cruise"));
    EXPECT_TRUE(r.bridge()->phase().startsWith("Cruise")) << r.bridge()->phase().toStdString();
    EXPECT_TRUE(r.bridge()->flightOpen());
    EXPECT_TRUE(r.bridge()->hasSample());
    EXPECT_FALSE(r.seek("nonsense"));
}

TEST(ReplaySession, SpeedAndPause) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, std::nullopt, "XLY", Rates{});
    ASSERT_TRUE(r.seek("0:10"));
    const auto before = r.bridge()->now();
    r.setSpeed(10);
    r.tick(std::chrono::seconds(1));
    EXPECT_EQ(r.bridge()->now() - before, std::chrono::seconds(10));
    r.setPaused(true);
    EXPECT_TRUE(r.paused());
    const auto paused = r.bridge()->now();
    r.tick(std::chrono::seconds(1));
    EXPECT_EQ(r.bridge()->now(), paused);
}

TEST(ReplaySession, RestartStartsClean) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, std::nullopt, "XLY", Rates{});
    ASSERT_TRUE(r.seek("block_in"));
    ASSERT_TRUE(r.finished());
    const RecorderBridge* old = r.bridge();
    r.restart();
    EXPECT_NE(r.bridge(), old);
    EXPECT_FALSE(r.finished());
    EXPECT_FALSE(r.bridge()->flightOpen());
    EXPECT_FALSE(r.bridge()->hasSample());
    EXPECT_EQ(r.bridge()->connection(), "replay");
    EXPECT_EQ(r.plannedFlight()->state(), "replay");
}

TEST(ReplaySession, MissingLogGivesAnError) {
    qtApp();
    const fs::path dir = fs::temp_directory_path() / "blocktime-replay-missing";
    fs::remove_all(dir);
    fs::create_directories(dir);
    ReplaySession r(dir, kReplayFlight, std::nullopt, "XLY", Rates{});
    EXPECT_EQ(r.error(), "The log for 25 Nov is no longer on this PC");
    EXPECT_FALSE(r.seek("block_out"));
}

TEST(ReplaySession, WritesNothing) {
    qtApp();
    const fs::path dir = fs::temp_directory_path() / "blocktime-replay-writes";
    fs::remove_all(dir);
    fs::create_directories(dir);
    fs::copy(kReplayFixtureDir, dir, fs::copy_options::recursive);
    const fs::path file = dir / "events-2026-11-25.jsonl";
    const auto size = fs::file_size(file);
    const auto written = fs::last_write_time(file);
    {
        ReplaySession r(dir, kReplayFlight, std::nullopt, "XLY", Rates{});
        ASSERT_TRUE(r.seek("block_in"));
    }
    EXPECT_EQ(fs::file_size(file), size);
    EXPECT_EQ(fs::last_write_time(file), written);
    EXPECT_EQ(std::distance(fs::directory_iterator(dir), fs::directory_iterator()), 1);
}

TEST(ReplaySession, TimerPlaysUntilPaused) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, std::nullopt, "XLY", Rates{});
    ASSERT_TRUE(r.seek("0:10"));
    r.setSpeed(10);
    const auto from = r.bridge()->now();
    r.start();
    EXPECT_TRUE(processUntil([&] { return r.bridge()->now() - from >= std::chrono::seconds(2); }));
    r.setPaused(true);
    QCoreApplication::processEvents();
    const auto paused = r.bridge()->now();
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (std::chrono::steady_clock::now() < until) QCoreApplication::processEvents();
    EXPECT_EQ(r.bridge()->now(), paused);
}

TEST(RecorderBridge, StatusLinesFollowEvents) {
    qtApp();
    RecorderBridge b;
    EXPECT_EQ(b.statusLines()[0].toMap()["text"].toString(), "Preparing aircraft for flight");
    blocktime::BlockOut out;
    out.flightId = "2026-11-25T18:43:04.053Z";
    b.applyBlockEvent(out);
    b.applyFlightEvent("phase_changed", phaseEvent("pushback", "18:43:10"));
    b.applyFlightEvent("liftoff", {{"time", "2026-11-25T18:55:00Z"}, {"runway", "14R"}});
    const QVariantList lines = b.statusLines();
    ASSERT_GE(lines.size(), 2);
    EXPECT_EQ(lines[0].toMap()["text"].toString(), "Departed runway 14R");
    EXPECT_FALSE(lines[0].toMap()["time"].toString().isEmpty());  // local h:mm AP
    EXPECT_EQ(lines[1].toMap()["text"].toString(), "Pushback and startup");
    EXPECT_EQ(b.pill()[1].toMap()["state"].toString(), "current");
    EXPECT_EQ(b.pillMarker()["section"].toInt(), 1);
    EXPECT_EQ(b.pillMarker()["parts"].toInt(), 3);
    EXPECT_EQ(b.pillMarker()["label"].toString(), "Pushback");
    EXPECT_TRUE(b.checklist().isEmpty());
    EXPECT_DOUBLE_EQ(b.lastSample().latitudeDeg, 0.0);

    blocktime::StatusContext c;
    c.departureRunway = "14R";
    b.setStatusContext(c);
    b.applyFlightEvent("phase_changed", phaseEvent("taxi_out", "18:56:00"));
    EXPECT_EQ(b.statusLines()[0].toMap()["text"].toString(), "Taxiing to runway 14R");
}

namespace {

// KSAN → KLAS, planned with a SimBrief plan: block 1:05 at FL330, parked at Gate 41.
PlannedFlightView headerView() {
    PlannedFlightView v;
    v.registration = "N814SX";
    v.typeCode = "B737-BBJ2";
    v.origin = "KSAN";
    v.originName = "San Diego International Airport";
    v.originCity = "San Diego";
    v.destination = "KLAS";
    v.destinationName = "Harry Reid International Airport";
    v.destinationCity = "Las Vegas";
    v.flightNumber = "1231";
    v.loadInParking = "Gate 41";
    v.route = {{32.7336, -117.19}, {36.08, -115.152}};
    SimbriefPlan p;
    p.cruiseAltitudeFt = 33000;
    p.estBlockS = 3900;
    p.originTransitionAltitudeFt = 18000;
    p.destinationTransitionLevelFt = 18000;
    p.originRunway = "27";
    v.plan = p;
    return v;
}

const char* kHeaderFlight = "2026-11-25T20:22:00.000Z";

// The header shows times in Dave's local time, "1:22 PM".
QString localText(const char* iso) { return QDateTime::fromString(iso, Qt::ISODateWithMs).toLocalTime().toString("h:mm AP"); }

DashboardHeader::SettledFlight noSettledFlight() {
    return [](const QString&) { return std::optional<PlannedFlightView>{}; };
}

}  // namespace

TEST(DashboardHeader, FromPlannedFlight) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(headerView(), "XLY", Rates{}, b, kHeaderFlight);
    DashboardHeader h(b, f, "XLY", "Soxxly Airlines", noSettledFlight());
    EXPECT_TRUE(h.visible());
    EXPECT_EQ(h.airlineIcao(), "XLY");
    EXPECT_EQ(h.flightNumber(), "1231");
    EXPECT_EQ(h.flightText(), "Flight 1231");
    EXPECT_EQ(h.citiesText(), "San Diego to Las Vegas");
    EXPECT_EQ(h.clockText(), "1:05");
    EXPECT_TRUE(h.clockIsPlan());
    EXPECT_EQ(h.originLine(), "Departing soon");
    EXPECT_EQ(h.originIcao(), "KSAN");
    EXPECT_EQ(h.destinationName(), "Harry Reid International Airport");
    EXPECT_TRUE(h.destinationLine().startsWith("Arrival ")) << h.destinationLine().toStdString();
    EXPECT_DOUBLE_EQ(h.progress(), 0.0);
}

TEST(DashboardHeader, InFlight) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(headerView(), "XLY", Rates{}, b, kHeaderFlight);
    DashboardHeader h(b, f, "XLY", "Soxxly Airlines", noSettledFlight());
    BlockOut out;
    out.flightId = kHeaderFlight;
    out.time = *parseUtc(kHeaderFlight);
    b.applyBlockEvent(out);
    EXPECT_EQ(h.originLine(), "Departing soon");  // until it leaves the runway (D-184)
    b.applyFlightEvent("liftoff", {{"time", "2026-11-25T20:31:00Z"}, {"runway", "27"}});
    b.applyFlightEvent("phase_changed", {{"phase", "cruise"}, {"time", "2026-11-25T20:48:00Z"}, {"alt_ft", 33000},
                                         {"flight_level", true}});
    TelemetrySample s{};
    s.latitudeDeg = 34.4;
    s.longitudeDeg = -116.17;
    s.indicatedAltitudeFt = 33000;
    s.groundSpeedKt = 452;
    b.applySample(s);
    EXPECT_FALSE(h.clockIsPlan());
    EXPECT_EQ(h.originLine(), "Departed " + localText(kHeaderFlight));  // the block out time
    EXPECT_GT(h.progress(), 0.4);
    EXPECT_LT(h.progress(), 0.6);
    const QRegularExpression distance(QString::fromUtf8(R"(^\d+ NM flown · \d+ NM to go · FL330 · 0 KT$)"));
    EXPECT_TRUE(distance.match(h.distanceText()).hasMatch()) << h.distanceText().toStdString();
    const QVariantList shares = h.arcShares();
    ASSERT_EQ(shares.size(), 100);
    EXPECT_DOUBLE_EQ(shares[static_cast<int>(h.progress() * 100)].toDouble(), 1.0);
    EXPECT_EQ(b.statusLines()[0].toMap()["text"].toString(), "Cruising at FL330");

    EXPECT_TRUE(h.destinationLine().startsWith("Arrival ")) << h.destinationLine().toStdString();
}

TEST(DashboardHeader, SummaryAtStartupUsesSettledFlight) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(headerView(), "XLY", Rates{}, b, kHeaderFlight);
    BlockIn in;
    in.flightId = kHeaderFlight;
    in.time = *parseUtc(kHeaderFlight);
    b.applyBlockEvent(in);  // the summary shows and the display holds no flight
    ASSERT_EQ(f.state(), "summary");
    ASSERT_FALSE(f.hasFlight());
    QString asked;
    DashboardHeader h(b, f, "XLY", "Soxxly Airlines", [&asked](const QString& id) {
        asked = id;
        return std::optional<PlannedFlightView>(headerView());
    });
    EXPECT_EQ(asked, kHeaderFlight);
    EXPECT_TRUE(h.visible());
    EXPECT_EQ(h.citiesText(), "San Diego to Las Vegas");
    // Settled before this session: no block events, so no clock or times, and the whole route flown (review finding 2).
    EXPECT_FALSE(h.clockIsPlan());
    EXPECT_EQ(h.clockText(), "--:--");
    EXPECT_EQ(h.originLine(), "Departed");
    EXPECT_EQ(h.destinationLine(), "Arrived");
    EXPECT_DOUBLE_EQ(h.progress(), 1.0);
}

TEST(DashboardHeader, InterruptResetsTheArc) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(headerView(), "XLY", Rates{}, b, kHeaderFlight);
    DashboardHeader h(b, f, "XLY", "Soxxly Airlines", noSettledFlight());
    BlockOut out;
    out.flightId = kHeaderFlight;
    out.time = *parseUtc(kHeaderFlight);
    b.applyBlockEvent(out);
    b.applyFlightEvent("liftoff", {{"time", "2026-11-25T20:31:00Z"}, {"runway", "27"}});
    TelemetrySample s{};
    s.latitudeDeg = 34.4;
    s.longitudeDeg = -116.17;
    s.indicatedAltitudeFt = 20000;
    b.applySample(s);
    ASSERT_GT(h.progress(), 0.0);
    FlightInterrupted gone;
    gone.flightId = kHeaderFlight;
    gone.reason = "sim_quit";
    b.applyBlockEvent(gone);
    EXPECT_DOUBLE_EQ(h.progress(), 0.0);
    for (const QVariant& v : h.arcShares()) EXPECT_DOUBLE_EQ(v.toDouble(), -1.0);
}

namespace {

// Dave's KSAN → KSJC plan with its navlog times and fuel (D-192): enroute 1:04:36; taxi 9 min out, 2 in (D-212).
constexpr int kKsjcEnrouteS = 3600 + 4 * 60 + 36;

PlannedFlightView ksjcView() {
    PlannedFlightView v = headerView();
    v.destination = "KSJC";
    v.destinationName = "Norman Y. Mineta San Jose International Airport";
    v.destinationCity = "San Jose";
    nlohmann::json ofp = nlohmann::json::parse(dataFile("ofp-ksan-ksjc-brief.json"));
    ofp["navlog"] = nlohmann::json::parse(dataFile("ofp-ksan-ksjc-navlog.json"))["navlog"];
    v.planFixes = blocktime::parsePlanFixes(ofp.dump());
    v.brief = blocktime::parsePlanBrief(ofp.dump());
    v.route = {{32.7336, -117.19}};
    for (const auto& fix : v.planFixes) v.route.push_back(fix.at);
    v.route.push_back({37.3626, -121.929});
    v.plan->cruiseAltitudeFt = 40000;
    v.plan->estBlockS = 6276;
    v.taxiOutS = 540;
    v.taxiInS = 120;
    return v;
}

QString localAt(std::chrono::system_clock::time_point t) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).toLocalTime().toString("h:mm AP");
}

// A header on its own bridge with a clock the test moves.
struct HeaderRig {
    RecorderBridge b;
    std::chrono::system_clock::time_point now = *parseUtc(kHeaderFlight);
    ReplayPlannedFlight f;
    DashboardHeader h;
    explicit HeaderRig(PlannedFlightView v)
        : f(std::move(v), "XLY", Rates{}, b, kHeaderFlight), h(b, f, "XLY", "Soxxly Airlines", noSettledFlight()) {
        b.setClock([this] { return now; });
    }
    void blockOut() {
        BlockOut out;
        out.flightId = kHeaderFlight;
        out.time = now;
        b.applyBlockEvent(out);
    }
    void liftoff() { b.applyFlightEvent("liftoff", {{"time", isoAt(now)}, {"runway", "27"}}); }
    void phase(const char* code) { b.applyFlightEvent("phase_changed", {{"phase", code}, {"time", isoAt(now)}}); }
    void at(double lat, double lon, double altFt = 30000, double iasKt = 280, double mach = 0.5) {
        TelemetrySample s{};
        s.latitudeDeg = lat;
        s.longitudeDeg = lon;
        s.indicatedAltitudeFt = altFt;
        s.indicatedAirspeedKt = iasKt;
        s.mach = mach;
        b.applySample(s);
    }
    static QString isoAt(std::chrono::system_clock::time_point t) {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
        return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).toString(Qt::ISODateWithMs);
    }
};

}  // namespace

// D-192: the arrivals board. Before block out the time has no ~ and no line under it.
TEST(DashboardHeader, ArrivalBeforeBlockOut) {
    qtApp();
    HeaderRig r(ksjcView());
    EXPECT_EQ(r.h.destinationLine(), "Arrival " + localAt(r.now + std::chrono::seconds(540 + kKsjcEnrouteS + 120)));
    EXPECT_EQ(r.h.punctualityText(), "");
    EXPECT_EQ(r.h.punctuality(), "");
}

TEST(DashboardHeader, OnTimeAtBlockOut) {
    qtApp();
    HeaderRig r(ksjcView());
    r.blockOut();
    r.now += std::chrono::seconds(10);
    EXPECT_EQ(r.h.punctualityText(), "On time");
    EXPECT_EQ(r.h.punctuality(), "on_time");
}

// Review Focus 2: holding, the arrival slips minute by minute and Delayed grows.
TEST(DashboardHeader, DelayedInAHold) {
    qtApp();
    const PlannedFlightView v = ksjcView();
    HeaderRig r(v);
    r.blockOut();
    r.now += std::chrono::seconds(540);
    r.liftoff();
    r.phase("climb");
    for (std::size_t k = 0; k <= 10; ++k) r.at(v.planFixes[k].at.lat, v.planFixes[k].at.lon);  // to SILCN
    r.phase("descent");
    r.now += std::chrono::seconds(47 * 60 + 9);  // SILCN on the plan's time: on time
    r.at(v.planFixes[10].at.lat, v.planFixes[10].at.lon);
    EXPECT_EQ(r.h.punctuality(), "on_time");
    const auto first = r.h.predictions().arrival;
    ASSERT_TRUE(first.has_value());
    r.now += std::chrono::minutes(30);
    r.at(v.planFixes[10].at.lat, v.planFixes[10].at.lon);
    const auto later = r.h.predictions().arrival;
    ASSERT_TRUE(later.has_value());
    EXPECT_EQ(std::chrono::duration_cast<std::chrono::seconds>(*later - *first).count(), 1800);
    EXPECT_EQ(r.h.punctuality(), "delayed");
    EXPECT_TRUE(r.h.punctualityText().startsWith("Delayed ~")) << r.h.punctualityText().toStdString();
}

TEST(DashboardHeader, ArrivedFreezes) {
    qtApp();
    const PlannedFlightView v = ksjcView();
    HeaderRig r(v);
    const auto out = r.now;
    r.blockOut();
    r.now += std::chrono::seconds(540);
    r.liftoff();
    r.at(v.planFixes[0].at.lat, v.planFixes[0].at.lon);
    r.now = out + std::chrono::seconds(540 + kKsjcEnrouteS);
    r.b.applyFlightEvent("touchdown", {{"time", HeaderRig::isoAt(r.now)}});
    EXPECT_FALSE(r.h.distanceText().contains("KT")) << r.h.distanceText().toStdString();  // after touchdown
    r.now = out + std::chrono::seconds(540 + kKsjcEnrouteS + 120 + 180);  // 3 minutes late
    BlockIn in;
    in.flightId = kHeaderFlight;
    in.time = r.now;
    in.parking = "Gate 4";
    r.b.applyBlockEvent(in);
    const QString arrived = "Arrived " + localAt(in.time) + QString::fromUtf8(" · Gate 4");
    EXPECT_EQ(r.h.destinationLine(), arrived);
    EXPECT_EQ(r.h.punctualityText(), "Delayed 3m");
    EXPECT_EQ(r.h.punctuality(), "delayed");
    r.now += std::chrono::hours(1);
    EXPECT_EQ(r.h.destinationLine(), arrived);
    EXPECT_EQ(r.h.punctualityText(), "Delayed 3m");
}

// D-210: the altitude and the indicated airspeed, from liftoff to touchdown.
TEST(DashboardHeader, DistanceLineAltitudeAndSpeed) {
    qtApp();
    const PlannedFlightView v = ksjcView();
    HeaderRig r(v);
    r.blockOut();
    r.at(v.route[0].lat, v.route[0].lon, 17, 12, 0.02);
    EXPECT_FALSE(r.h.distanceText().contains("KT")) << r.h.distanceText().toStdString();  // on the ground
    r.liftoff();
    r.phase("climb");
    r.at(v.planFixes[0].at.lat, v.planFixes[0].at.lon, 12000, 290, 0.5);
    EXPECT_TRUE(r.h.distanceText().endsWith(QString::fromUtf8(" · 12,000 FT · 290 KT")))
        << r.h.distanceText().toStdString();
    r.phase("cruise");
    r.at(v.planFixes[1].at.lat, v.planFixes[1].at.lon, 35000, 260, 0.78);
    EXPECT_TRUE(r.h.distanceText().endsWith(QString::fromUtf8(" · FL350 · M.78"))) << r.h.distanceText().toStdString();
}

TEST(DashboardHeader, TodShare) {
    qtApp();
    HeaderRig withPlan(ksjcView());
    EXPECT_GT(withPlan.h.todShare(), 0.0);
    EXPECT_LT(withPlan.h.todShare(), 1.0);
    HeaderRig noFixes(headerView());
    EXPECT_DOUBLE_EQ(noFixes.h.todShare(), -1.0);
}

// Review finding 1: between samples the clock moves but the distance flown doesn't. Airborne, the predictions run on
// the last sample's time, so the 1 s tick can't saw-tooth the arrival across a minute or the 2-minute band.
TEST(DashboardHeader, ArrivalStillBetweenSamples) {
    qtApp();
    const PlannedFlightView v = ksjcView();
    HeaderRig r(v);
    r.blockOut();
    r.now += std::chrono::seconds(540);
    r.liftoff();
    r.phase("climb");
    r.at(v.planFixes[0].at.lat, v.planFixes[0].at.lon);
    const auto atSample = r.h.predictions().arrival;
    ASSERT_TRUE(atSample.has_value());
    r.now += std::chrono::milliseconds(900);  // the timer's tick, before the next sample
    EXPECT_EQ(r.h.predictions().arrival, atSample);
    r.now += std::chrono::seconds(30);  // no samples for 30 s: a sim freeze holds the estimate
    EXPECT_EQ(r.h.predictions().arrival, atSample);
}

// Review Focus 1: a plan without navlog times keeps the old estimate and predicts nothing.
TEST(DashboardHeader, NoNavlogTimesFallsBack) {
    qtApp();
    HeaderRig r(headerView());
    EXPECT_EQ(r.h.destinationLine(), "Arrival " + localAt(r.now + std::chrono::seconds(3900)));
    EXPECT_FALSE(r.h.predictions().tocNm.has_value());
    r.blockOut();
    EXPECT_EQ(r.h.punctualityText(), "On time");
}

TEST(DashboardHeader, NextFlightStartsOver) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(headerView(), "XLY", Rates{}, b, kHeaderFlight);
    DashboardHeader h(b, f, "XLY", "Soxxly Airlines", noSettledFlight());
    BlockOut out;
    out.flightId = kHeaderFlight;
    out.time = *parseUtc(kHeaderFlight);
    b.applyBlockEvent(out);
    b.applyFlightEvent("liftoff", {{"time", "2026-11-25T20:31:00Z"}, {"runway", "27"}});
    BlockIn in;
    in.flightId = kHeaderFlight;
    in.time = out.time + std::chrono::hours(1);
    in.parking = "Gate C11";
    b.applyBlockEvent(in);
    ASSERT_TRUE(h.destinationLine().startsWith("Arrived ")) << h.destinationLine().toStdString();
    BlockOut next;  // the same airframe flies again before the summary changes the flight
    next.flightId = "2026-11-25T22:00:00.000Z";
    next.time = *parseUtc("2026-11-25T22:00:00.000Z");
    b.applyBlockEvent(next);
    EXPECT_EQ(h.originLine(), "Departing soon");
    EXPECT_TRUE(h.destinationLine().startsWith("Arrival ")) << h.destinationLine().toStdString();
    EXPECT_TRUE(b.blockInTime().isNull());
}

// The recorder logs phase_changed arrived in the same instant as block_in, after it; the replay plays it too, so the
// header's pill reaches Arrival (milestone 12).
TEST(ReplaySession, BlockInPlaysWhatCameWithIt) {
    qtApp();
    const fs::path dir = fs::temp_directory_path() / "blocktime-replay-arrived";
    fs::remove_all(dir);
    fs::create_directories(dir);
    std::ifstream in(kReplayFixtureDir / "events-2026-11-25.jsonl");
    std::ofstream out(dir / "events-2026-11-25.jsonl");
    std::string line;
    while (std::getline(in, line)) {
        out << line << "\n";
        if (line.find("\"type\":\"block_in\"") != std::string::npos) {
            const std::string ts = line.substr(7, 24);  // {"ts":"2026-11-25T20:01:45.912Z"
            out << R"({"ts":")" << ts << R"(","type":"phase_changed","data":{"alt_ft":55,"flight_id":")"
                << kReplayFlight.toStdString() << R"(","flight_level":false,"phase":"arrived","previous":"taxi_in",)"
                << R"("time":")" << ts << "\"}}\n";
        }
    }
    out.close();
    ReplaySession r(dir, kReplayFlight, std::nullopt, "XLY", Rates{});
    ASSERT_TRUE(r.seek("block_in"));
    EXPECT_TRUE(r.finished());
    EXPECT_EQ(r.bridge()->pill()[5].toMap()["state"].toString(), "current");
    EXPECT_EQ(r.bridge()->pill()[4].toMap()["state"].toString(), "done");
}

namespace {

// The real YourFlightModel's order at settling: reload() empties the flight (state "") before the summary id is
// set; ReplayPlannedFlight goes to the summary in one step.
class SettlingDisplay : public PlannedFlightDisplay {
public:
    explicit SettlingDisplay(PlannedFlightView v) : PlannedFlightDisplay("XLY", true, nullptr) { flight_ = std::move(v); }
    QString state() const override {
        if (!summaryFlightId_.isEmpty()) return QStringLiteral("summary");
        return flight_ ? QStringLiteral("flying") : QString();
    }
    YourFlightStage stage() const override {
        if (!summaryFlightId_.isEmpty()) return YourFlightStage::Summary;
        return flight_ ? YourFlightStage::Flying : YourFlightStage::Empty;
    }
    void settle(const QString& id) {
        flight_.reset();
        emit changed();  // reload()
        summaryFlightId_ = id;
        emit changed();
    }
};

// A flight source set to any stage, for the panels' visibility (review 3.1).
class StagedSource : public FlightSource {
public:
    explicit StagedSource(PlannedFlightView v) : FlightSource(nullptr), view_(std::move(v)) {}
    const std::optional<PlannedFlightView>& view() const override { return view_; }
    YourFlightStage stage() const override { return stage_; }
    QString summaryFlightId() const override { return {}; }
    void setStage(YourFlightStage s) {
        stage_ = s;
        emit changed();
    }

private:
    std::optional<PlannedFlightView> view_;
    YourFlightStage stage_ = YourFlightStage::Empty;
};

}  // namespace

// The header from Accept on, Pre-Flight while accepted, the in-flight panel from block out to Settle Up.
TEST(QtModels, HeaderVisibleFromAccepted) {
    qtApp();
    RecorderBridge b;
    StagedSource f(headerView());
    DashboardHeader h(b, f, "XLY", "Soxxly Airlines", noSettledFlight());
    PreFlightModel pre(b, f);
    InFlightModel in(b, f, 3, &h);
    using S = YourFlightStage;
    struct Row {
        S stage;
        bool header, preflight, inflight;
    };
    for (const Row& r : {Row{S::Empty, false, false, false}, Row{S::Form, false, false, false},
                         Row{S::Dispatched, false, false, false}, Row{S::Imported, false, false, false},
                         Row{S::Accepted, true, true, false}, Row{S::Flying, true, false, true},
                         Row{S::Arrived, true, false, true}, Row{S::Summary, true, false, false}}) {
        f.setStage(r.stage);
        const std::string name(stageName(r.stage));
        EXPECT_EQ(h.visible(), r.header) << name;
        EXPECT_EQ(pre.visible(), r.preflight) << name;
        EXPECT_EQ(in.visible(), r.inflight) << name;
    }
}

TEST(DashboardHeader, SettlingKeepsTheFlight) {
    qtApp();
    RecorderBridge b;
    SettlingDisplay f(headerView());
    DashboardHeader h(b, f, "XLY", "Soxxly Airlines", noSettledFlight());
    BlockOut out;
    out.flightId = kHeaderFlight;
    out.time = *parseUtc(kHeaderFlight);
    b.applyBlockEvent(out);
    b.applyFlightEvent("liftoff", {{"time", "2026-11-25T20:31:00Z"}, {"runway", "27"}});
    TelemetrySample s{};
    s.latitudeDeg = 34.4;
    s.longitudeDeg = -116.17;
    s.indicatedAltitudeFt = 33000;
    b.applySample(s);
    BlockIn in;
    in.flightId = kHeaderFlight;
    in.time = out.time + std::chrono::hours(1);
    in.parking = "Gate C11";
    b.applyBlockEvent(in);
    f.settle(kHeaderFlight);  // the model settles after the bridge's block in, as in the app
    EXPECT_TRUE(h.visible());
    EXPECT_EQ(h.originLine(), "Departed " + localText(kHeaderFlight));
    EXPECT_DOUBLE_EQ(h.progress(), 1.0);
    const QVariantList shares = h.arcShares();
    EXPECT_TRUE(std::any_of(shares.begin(), shares.end(), [](const QVariant& v) { return v.toDouble() >= 0; }));
    EXPECT_EQ(h.destinationLine(), "Arrived " + localText("2026-11-25T21:22:00.000Z") + QString::fromUtf8(" · Gate C11"));
    EXPECT_FALSE(h.clockIsPlan());
}

// ---- The Pre-Flight panel (milestone 13, D-153, D-188, D-189) ----

namespace {

// N814SX's KSAN → KSJC leg of 2026-11-25 as planned: runway 27, PADRZ2 IKAYE, 44 passengers.
PlannedFlightView preflightView(bool withPlan = true) {
    PlannedFlightView v;
    v.registration = "N814SX";
    v.origin = "KSAN";
    v.destination = "KSJC";
    v.flightNumber = "8";
    v.kind = FlightKind::Vip;
    v.plannedAt = "2026-11-25T18:00:00Z";
    v.originRunway = "27";
    v.cabins = {{"economy", 44}};
    v.holds = {{"forward", 7842, 4}, {"aft", 9803, 5}};
    v.route = {{32.7336, -117.19}, {37.3626, -121.929}};
    if (withPlan) {
        SimbriefPlan p;
        p.rampFuelLb = 13439;
        p.cruiseAltitudeFt = 40000;
        p.originTransitionAltitudeFt = 18000;
        p.originRunway = "27";
        p.estBlockS = 4200;
        v.plan = p;
        v.brief = blocktime::parsePlanBrief(dataFile("ofp-ksan-ksjc-brief.json"));
    }
    return v;
}

QVariantMap variantOf(const nlohmann::json& j) {
    return QJsonDocument::fromJson(QByteArray::fromStdString(j.dump())).object().toVariantMap();
}

nlohmann::json ksanClearance() {
    return {{"flight_id", ""},
            {"entry",
             {{"id", 110637058},
              {"channel", "COM1"},
              {"ident", "KSAN"},
              {"position", "CLEARANCE"},
              {"stamp_zulu", "2026-11-25T18:39:40Z"},
              {"frequency", "125.9"},
              {"outgoing_message",
               "Cleared to Norman Y. Mineta San Jose via the PADRZ2 departure. Then as filed. Climb via the SID. Expect "
               "FL400 one-zero minutes after departure. Departure on 119.6. Squawk 3166. Expect runway 27"},
              {"incoming_message", "Ready to copy our IFR clearance."}}}};
}

std::chrono::system_clock::time_point utc(const char* iso) { return *blocktime::parseUtc(iso); }

}  // namespace

TEST(PreFlightModel, WaitingForPlan) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(false), "XLY", Rates{}, b, kReplayFlight);
    PreFlightModel m(b, f);
    EXPECT_TRUE(m.visible());
    EXPECT_FALSE(m.hasPlan());
    EXPECT_EQ(m.blockFuelText(), "");  // QML draws the dim dash
    EXPECT_EQ(m.zfwText(), "");
    EXPECT_EQ(m.cargoText(), "");
    EXPECT_TRUE(m.holds().isEmpty());
    ASSERT_EQ(m.passengers().size(), 1);
    EXPECT_EQ(m.passengers()[0].toMap()["label"].toString(), "Passengers");
    EXPECT_EQ(m.passengers()[0].toMap()["value"].toString(), "44");
    EXPECT_EQ(m.runway(), "27");  // the planned flight's runway from New Flight's ATIS
    EXPECT_EQ(m.sid(), "");
    EXPECT_FALSE(m.cleared());
}

TEST(PreFlightModel, LoadSheetAndPlanSid) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    PreFlightModel m(b, f);
    EXPECT_TRUE(m.hasPlan());
    EXPECT_EQ(m.blockFuelText(), "13,439 lb");
    EXPECT_EQ(m.zfwText(), "112,967 lb");
    EXPECT_EQ(m.cargoText(), "2,420 lb");
    ASSERT_EQ(m.holds().size(), 2);
    EXPECT_EQ(m.holds()[0].toMap()["label"].toString(), "Forward");
    EXPECT_EQ(m.holds()[0].toMap()["value"].toString(), "1,075 lb");
    EXPECT_EQ(m.holds()[1].toMap()["value"].toString(), "1,345 lb");
    EXPECT_EQ(m.cruiseText(), "FL400");
    EXPECT_EQ(m.transitionText(), "18,000");
    EXPECT_EQ(m.runway(), "27");
    EXPECT_EQ(m.runwayTag(), "PLAN");
    EXPECT_EQ(m.headingText(), "275°");
    EXPECT_EQ(m.conditionText(), "Dry");
    EXPECT_EQ(m.sid(), "PADRZ2 IKAYE");  // SimBrief's until ATC changes it (D-189)
    EXPECT_EQ(m.sidTag(), "PLAN");
    EXPECT_FALSE(m.countdownVisible());
}

TEST(PreFlightModel, ClearanceAndCountdown) {
    qtApp();
    RecorderBridge b;
    auto now = utc("2026-11-25T18:43:00Z");  // 3:20 after the clearance
    b.setClock([&now] { return now; });
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    PreFlightModel m(b, f);
    b.applyFlightEvent("atc_exchange", variantOf(ksanClearance()));
    EXPECT_TRUE(m.cleared());
    EXPECT_EQ(m.squawk(), "3166");
    EXPECT_EQ(m.initialAltitude(), "Climb via SID");
    EXPECT_EQ(m.sid(), "PADRZ2 IKAYE");
    EXPECT_EQ(m.sidTag(), "CLEARED");
    EXPECT_EQ(m.runwayTag(), "ATC");
    EXPECT_TRUE(m.countdownVisible());
    EXPECT_EQ(m.countdownText(), "26:40");
    EXPECT_FALSE(m.countdownUrgent());
    now = utc("2026-11-25T19:05:00Z");
    EXPECT_TRUE(m.countdownUrgent());

    blocktime::BlockOut out;
    out.flightId = kReplayFlight.toStdString();
    b.applyBlockEvent(out);
    EXPECT_FALSE(m.countdownVisible());
}

TEST(PreFlightModel, AtcChangesTheRunway) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    PreFlightModel m(b, f);
    auto c = ksanClearance();
    c["entry"]["outgoing_message"] = "Cleared to San Jose via the BORDR2 departure. Squawk 3166. Expect runway 9";
    b.applyFlightEvent("atc_exchange", variantOf(c));
    EXPECT_EQ(m.runway(), "09");
    EXPECT_TRUE(m.runwayChanged());
    EXPECT_EQ(m.runwayTag(), "PLAN 27");
    EXPECT_EQ(m.headingText(), "095°");
    EXPECT_EQ(m.sid(), "BORDR2");
    EXPECT_TRUE(m.sidChanged());
    EXPECT_EQ(m.sidTag(), "PLAN PADRZ2");
}

// D-194: the in-flight panel takes over at block out (it was liftoff, D-188).
TEST(PreFlightModel, HidesAtBlockOut) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    PreFlightModel m(b, f);
    EXPECT_TRUE(m.visible());
    blocktime::BlockOut out;
    out.flightId = kReplayFlight.toStdString();
    b.applyBlockEvent(out);
    EXPECT_FALSE(m.visible());
}

// D-193, D-201: the ATIS letter as its ICAO word.
TEST(PreFlightModel, AtisLetterIsAWord) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    PreFlightModel m(b, f);
    b.applyFlightEvent("atis", variantOf({{"role", "origin"}, {"icao", "KSAN"}, {"letter", "L"},
                                          {"altimeter", "30.09"}, {"text", "INFO LIMA. WIND CALM."}}));
    EXPECT_EQ(m.atisLetter(), "LIMA");
}

// 0.15.1: where to load in, as a readout (the planned flight panel's line is below the fold).
TEST(PreFlightModel, GateIsWhereItLastParked) {
    qtApp();
    RecorderBridge b;
    PlannedFlightView v = preflightView();
    v.loadInParking = "Gate 4";
    ReplayPlannedFlight f(v, "XLY", Rates{}, b, kReplayFlight);
    PreFlightModel m(b, f);
    EXPECT_EQ(m.gate(), "4");
    v.loadInParking = "";
    ReplayPlannedFlight unknown(v, "XLY", Rates{}, b, kReplayFlight);
    PreFlightModel none(b, unknown);
    EXPECT_EQ(none.gate(), "");
}

TEST(PreFlightModel, ReplayHasOne) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, preflightView(), "XLY", Rates{});
    ASSERT_NE(r.preflight(), nullptr);
    EXPECT_TRUE(r.preflight()->visible());
    ASSERT_TRUE(r.seek("taxi_out"));
    EXPECT_TRUE(r.preflight()->cleared());
    EXPECT_EQ(r.preflight()->squawk(), "3166");
    ASSERT_TRUE(r.seek("climb"));
    EXPECT_FALSE(r.preflight()->visible());
}

// ---- The in-flight panel (milestone 14, D-194..D-198, D-203) ----

namespace {

blocktime::TelemetrySample sampleOf(bool onGround, double gsKt, double iasKt, double aglFt = 0, double fuelLb = 13420) {
    blocktime::TelemetrySample s{};
    s.onGround = onGround ? 1 : 0;
    s.groundSpeedKt = gsKt;
    s.indicatedAirspeedKt = iasKt;
    s.altitudeAglFt = aglFt;
    s.fuelTotalLb = fuelLb;
    s.numEngines = 2;
    s.engCombustion[0] = s.engCombustion[1] = 1;
    return s;
}

void blockOutOf(RecorderBridge& b) {
    blocktime::BlockOut out;
    out.flightId = kReplayFlight.toStdString();
    out.fuelLb = 13420;
    b.applyBlockEvent(out);
}

}  // namespace

// 0.15.1: the arrival view stays after block in, its readouts intact, until Dave clicks Finish Flight.
TEST(InFlightModel, VisibleFromBlockOutUntilFinishFlight) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    InFlightModel m(b, f, 3);
    EXPECT_FALSE(m.visible());
    EXPECT_FALSE(m.finishable());
    blockOutOf(b);
    EXPECT_TRUE(m.visible());
    EXPECT_FALSE(m.finishable());
    const QString souls = m.readouts()[0].toMap()["value"].toString();
    ASSERT_FALSE(souls.isEmpty());
    blocktime::BlockIn in;
    in.flightId = kReplayFlight.toStdString();
    b.applyBlockEvent(in);
    EXPECT_TRUE(m.visible());
    EXPECT_TRUE(m.finishable());
    EXPECT_EQ(m.readouts()[0].toMap()["value"].toString(), souls);  // SOULS, from the flight that just ended
    m.finishFlight();
    EXPECT_FALSE(m.visible());
    EXPECT_FALSE(m.finishable());
}

// The next block out (a replay seek back, or a new flight) starts without the old flight's Finish Flight.
TEST(InFlightModel, BlockOutClearsFinishFlight) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    InFlightModel m(b, f, 3);
    blockOutOf(b);
    blocktime::BlockIn in;
    in.flightId = kReplayFlight.toStdString();
    b.applyBlockEvent(in);
    ASSERT_TRUE(m.finishable());
    blockOutOf(b);
    EXPECT_FALSE(m.finishable());
}

TEST(InFlightModel, SpeedFaceSwitchesAt40Kt) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    InFlightModel m(b, f, 3);
    blockOutOf(b);
    b.applySample(sampleOf(true, 39, 30));
    EXPECT_EQ(m.speedFace(), "gs");
    EXPECT_DOUBLE_EQ(m.speedKt(), 39);
    b.applySample(sampleOf(true, 41, 38));  // the takeoff roll
    EXPECT_EQ(m.speedFace(), "ias");
    EXPECT_DOUBLE_EQ(m.speedKt(), 38);
    b.applySample(sampleOf(false, 160, 150));
    EXPECT_EQ(m.speedFace(), "ias");
    b.applySample(sampleOf(true, 39, 35));  // the landing rollout
    EXPECT_EQ(m.speedFace(), "gs");
}

TEST(InFlightModel, SpeedBugVrefOnlyOnApproach) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    InFlightModel m(b, f, 3);
    blockOutOf(b);
    b.applyFlightEvent("cockpit", variantOf({{"pmdg", true}, {"seat_belts", 2}, {"vref_kt", 129}, {"landing_flaps", 40}}));
    b.applyFlightEvent("phase_changed", variantOf({{"phase", "climb"}}));
    EXPECT_DOUBLE_EQ(m.speedBugKt(), -1);
    b.applyFlightEvent("phase_changed", variantOf({{"phase", "approach"}}));
    EXPECT_DOUBLE_EQ(m.speedBugKt(), 129);
    b.applyFlightEvent("touchdown", variantOf({{"time", "2026-11-25T19:58:59.431Z"}, {"vs_fpm", -100.0}}));
    EXPECT_DOUBLE_EQ(m.speedBugKt(), -1);
}

TEST(InFlightModel, FlashRunsOnlyWhileWrong) {
    qtApp();
    RecorderBridge b;
    auto now = utc("2026-11-25T18:43:04Z");
    b.setClock([&now] { return now; });
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    InFlightModel m(b, f, 3);
    m.setFlashInterval(std::chrono::milliseconds(5));
    int flashes = 0;
    QObject::connect(&m, &InFlightModel::flashChanged, [&flashes] { ++flashes; });
    b.applySample(sampleOf(true, 0, 0));  // the beacon never switched on
    blockOutOf(b);
    b.applySample(sampleOf(true, 0, 0));  // not wrong yet: 10 s to switch it on
    QElapsedTimer t;
    t.start();
    while (t.elapsed() < 60) QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    EXPECT_EQ(flashes, 0);
    EXPECT_TRUE(m.flashOn());
    now += std::chrono::seconds(30);
    b.applySample(sampleOf(true, 0, 0));  // still off 30 s after block out: wrong
    t.restart();
    while (t.elapsed() < 60) QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    EXPECT_GT(flashes, 1);
}

TEST(InFlightModel, AltitudeFaceFollowsTheSamples) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    InFlightModel m(b, f, 3);
    blockOutOf(b);
    EXPECT_EQ(m.altitudeFace(), "radio");
    b.applySample(sampleOf(false, 250, 250, 3100));
    EXPECT_EQ(m.altitudeFace(), "altimeter");
    b.applyFlightEvent("flight_marker", variantOf({{"marker", "final_approach"}}));
    EXPECT_EQ(m.altitudeFace(), "altimeter");  // 0.15.1: the radio face only from 300 ft (D-209)
    b.applySample(sampleOf(false, 140, 140, 290));
    EXPECT_EQ(m.altitudeFace(), "radio");
    EXPECT_DOUBLE_EQ(m.aglFt(), 290);
    b.applyFlightEvent("phase_changed", variantOf({{"phase", "go_around"}}));
    EXPECT_EQ(m.altitudeFace(), "altimeter");
    b.applySample(sampleOf(false, 150, 150, 250));
    EXPECT_EQ(m.altitudeFace(), "altimeter");
    b.applySample(sampleOf(false, 180, 180, 1500));
    b.applySample(sampleOf(false, 140, 140, 290));
    EXPECT_EQ(m.altitudeFace(), "radio");
}

TEST(InFlightModel, FuelText) {
    qtApp();
    RecorderBridge b;
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    InFlightModel m(b, f, 3);
    blockOutOf(b);
    b.applySample(sampleOf(false, 250, 250, 20000, 7412.6));
    EXPECT_EQ(m.fuelText(), "7,410");
    EXPECT_DOUBLE_EQ(m.fuelScaleLb(), 18000);  // 13,420 at block out, rounded up to 6,000 lb
}

TEST(InFlightModel, KsanKsjcReplay) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, preflightView(), "XLY", Rates{});
    ASSERT_NE(r.inflight(), nullptr);
    // 19:53:04Z: the approach call (ATC's 19:50:26, logged at the 19:51:15 fetch) and the traffic call after it.
    ASSERT_TRUE(r.seek("1:10"));
    const InFlightModel& m = *r.inflight();
    EXPECT_TRUE(m.visible());
    EXPECT_EQ(m.phaseTitle(), "ARRIVAL");
    int latest = 0;
    for (const QVariant& s : m.atcSlots()) latest += s.toMap()["latest"].toBool() ? 1 : 0;
    EXPECT_EQ(latest, 3);
    bool julietT = false;
    for (const QVariant& rd : m.readouts()) {
        const QVariantMap map = rd.toMap();
        if (map["title"].toString() == "ATIS") julietT = map["value"].toString() == "JULIETT";
    }
    EXPECT_TRUE(julietT);
    EXPECT_EQ(m.readouts()[0].toMap()["title"].toString(), "SOULS");
}

TEST(InFlightModel, FirstFillIsNotAChange) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, preflightView(), "XLY", Rates{});
    QCoreApplication::processEvents();
    EXPECT_TRUE(r.inflight()->animate());
    ASSERT_TRUE(r.seek("cruise"));
    EXPECT_FALSE(r.inflight()->animate());  // a seek is a rebuild: nothing flips
    QCoreApplication::processEvents();
    EXPECT_TRUE(r.inflight()->animate());
}

// Review fix: a card checks a change with Qt.callLater, queued during the seek's burst. It must still see animate
// off, so a seek in an open replay window runs no flip wave (D-198).
TEST(InFlightModel, SeekIsCheckedWhileAnimateIsOff) {
    qtApp();
    ReplaySession r(kReplayFixtureDir, kReplayFlight, preflightView(), "XLY", Rates{});
    QCoreApplication::processEvents();
    InFlightModel* m = r.inflight();
    std::vector<bool> seen;
    QObject::connect(m, &InFlightModel::changed, m, [&seen, m] {
        QMetaObject::invokeMethod(m, [&seen, m] { seen.push_back(m->animate()); }, Qt::QueuedConnection);
    });
    ASSERT_TRUE(r.seek("cruise"));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    ASSERT_FALSE(seen.empty());
    for (bool animate : seen) EXPECT_FALSE(animate);
    EXPECT_TRUE(m->animate());
}

// Review fix: after a sim crash the same planned flight is flown again (D-108). The first attempt's windows must not
// carry over: the APU on at the second pushback and the altimeters on QNH are right, not wrong.
TEST(InFlightModel, InterruptedFlightStartsTheLampsAgain) {
    qtApp();
    RecorderBridge b;
    auto now = utc("2026-11-25T18:43:04Z");
    b.setClock([&now] { return now; });
    ReplayPlannedFlight f(preflightView(), "XLY", Rates{}, b, kReplayFlight);
    InFlightModel m(b, f, 3);
    const auto apuOnQnh = [&b] {
        b.applyFlightEvent("cockpit", variantOf({{"pmdg", true}, {"seat_belts", 2}, {"apu_switch", true}}));
        b.applyFlightEvent("altimeter", variantOf({{"capt_mb", 1019.0}, {"fo_mb", 1019.0}}));
    };
    blocktime::TelemetrySample beaconOn = sampleOf(true, 0, 0);
    beaconOn.lightBeacon = 1;
    b.applySample(beaconOn);
    blockOutOf(b);
    b.applyFlightEvent("phase_changed", variantOf({{"phase", "taxi_out"}}));
    b.applyFlightEvent("flight_marker", variantOf({{"marker", "transition_altitude"}}));  // then the sim crashed
    blocktime::FlightInterrupted interrupted;
    interrupted.flightId = kReplayFlight.toStdString();
    interrupted.reason = "sim_stopped";
    b.applyBlockEvent(interrupted);
    now += std::chrono::minutes(20);
    apuOnQnh();
    b.applySample(beaconOn);
    blockOutOf(b);
    now += std::chrono::minutes(2);
    b.applySample(beaconOn);
    for (const QVariant& lamp : m.lamps()) {
        EXPECT_FALSE(lamp.toMap()["wrong"].toBool()) << lamp.toMap()["name"].toString().toStdString();
    }
}

// ---- The predictions on the in-flight panel (milestone 15, D-192, D-213) ----

namespace {

QVariantMap readoutTitled(const InFlightModel& m, const char* title) {
    for (const QVariant& v : m.readouts()) {
        if (v.toMap()["title"].toString() == title) return v.toMap();
    }
    return {};
}

}  // namespace

// The airspeed gauge's bug: the plan's cruise IAS in cruise, VREF on approach, none in the climb.
TEST(InFlightModel, SpeedBug) {
    qtApp();
    const PlannedFlightView v = ksjcView();
    HeaderRig r(v);
    InFlightModel m(r.b, r.f, 3, &r.h);
    r.blockOut();
    r.now += std::chrono::seconds(540);
    r.liftoff();
    r.phase("climb");
    for (std::size_t k = 0; k <= 5; ++k) r.at(v.planFixes[k].at.lat, v.planFixes[k].at.lon);  // to TOC
    EXPECT_DOUBLE_EQ(m.speedBugKt(), -1);
    r.phase("cruise");
    r.at(v.planFixes[5].at.lat, v.planFixes[5].at.lon);
    EXPECT_DOUBLE_EQ(m.speedBugKt(), 240);  // TESEA's planned IAS, the next fix
    r.b.applyFlightEvent("cockpit", variantOf({{"pmdg", true}, {"vref_kt", 129}, {"landing_flaps", 40}}));
    r.phase("approach");
    EXPECT_DOUBLE_EQ(m.speedBugKt(), 129);
}

// The fuel gauge's bug: the plan's fuel here, from block out to touchdown.
TEST(InFlightModel, PlanFuelBug) {
    qtApp();
    HeaderRig r(ksjcView());
    InFlightModel m(r.b, r.f, 3, &r.h);
    EXPECT_DOUBLE_EQ(m.planFuelLb(), -1);  // at the gate the panel isn't up
    r.blockOut();
    r.at(32.7336, -117.19, 17, 0, 0);
    EXPECT_DOUBLE_EQ(m.planFuelLb(), 13439 - 500);  // the takeoff fuel
    r.now += std::chrono::seconds(540);
    r.liftoff();
    r.now += std::chrono::seconds(3600);
    r.b.applyFlightEvent("touchdown", {{"time", HeaderRig::isoAt(r.now)}, {"vs_fpm", -100.0}});
    EXPECT_DOUBLE_EQ(m.planFuelLb(), -1);
}

TEST(InFlightModel, PredictionReadoutsFollowTheHeader) {
    qtApp();
    const PlannedFlightView v = ksjcView();
    HeaderRig r(v);
    InFlightModel m(r.b, r.f, 3, &r.h);
    r.blockOut();
    r.now += std::chrono::seconds(540);
    r.liftoff();
    r.phase("climb");
    r.at(v.planFixes[0].at.lat, v.planFixes[0].at.lon);
    const QVariantMap first = readoutTitled(m, "TOP OF DESCENT");
    ASSERT_FALSE(first["value"].toString().isEmpty());
    ASSERT_TRUE(first["ticks"].toBool());
    r.now += std::chrono::minutes(5);
    r.at(v.planFixes[0].at.lat, v.planFixes[0].at.lon);  // holding: the next sample 5 minutes on
    emit r.h.changed();                                  // and the header's tick
    const QVariantMap later = readoutTitled(m, "TOP OF DESCENT");
    EXPECT_EQ(later["value"], first["value"]);
    EXPECT_NE(later["tag"], first["tag"]);  // the time slipped 5 minutes
    EXPECT_FALSE(readoutTitled(m, "LANDING")["tag"].toString().isEmpty());  // PLAN …
}

// Review 4.4: the one adapter from money lines to QML. Lines read back from the ledger have no working.
TEST(MoneyLinesToQml, CarriesTheWorking) {
    MoneyLine priced{"pilots", "Pilots", -125000};
    priced.quantities = {{2.5, "h"}, {2, "pilots"}};
    priced.rateDollars = 250;
    const QVariantList out = moneyLinesToQml({priced, MoneyLine{"lease", "Lease", -50000}});
    ASSERT_EQ(out.size(), 2);
    const QVariantMap first = out[0].toMap();
    EXPECT_EQ(first["category"].toString(), "pilots");
    EXPECT_EQ(first["label"].toString(), "Pilots");
    EXPECT_EQ(first["amount"].toString(), "-$1,250");
    EXPECT_EQ(first["working"].toString(), QString::fromUtf8("2.5 h × 2 pilots × $250"));
    EXPECT_EQ(out[1].toMap()["working"].toString(), "");
    EXPECT_EQ(out[1].toMap()["amount"].toString(), "-$500");
}

// --- AddAircraftModel (D-242, D-245) -------------------------------------------------------------------------------

namespace {

std::string readTestFile(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

// The test airline with 10 hours flown, so a third slot is open.
db::Database airlineWithASlot() {
    db::Database db = test::testAirline();
    EXPECT_TRUE(db.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb)"
        " SELECT 'ten', airplane_id, '1', 'vip', 1, 2, 2, 0, 4100, '2026-11-23T00:00:00Z',"
        " '2026-11-23T01:00:00Z', '2026-11-23T11:00:00Z', 0, 20000, 5000 FROM airplane WHERE registration = 'N814SX'"));
    return db;
}

const blocktime::Catalog& shippedAircraft() {
    static const blocktime::Catalog catalog = blocktime::loadCatalogDir(BLOCKTIME_SOURCE_DIR "/data/aircraft", nullptr);
    return catalog;
}

blocktime::Date day27() { return blocktime::Date{std::chrono::year{2026}, std::chrono::month{11}, std::chrono::day{27}}; }

QVariantMap fieldNamed(const AddAircraftModel& m, const QString& key) {
    for (const QVariant& f : m.fields()) {
        if (f.toMap()["key"].toString() == key) return f.toMap();
    }
    return {};
}

}  // namespace

TEST(AddAircraft, ReadFillsFormAndPreselects) {
    db::Database db = airlineWithASlot();
    FakeReviewClient claude;
    AddAircraftModel m(db, shippedAircraft(), kRatesFile, claude, day27);
    m.readImage(QByteArray("png bytes"), "image/png");
    EXPECT_EQ(m.state(), "reading");
    ASSERT_EQ(claude.bodies.size(), 1u);
    claude.answer(200, readTestFile("airframe_read_ok.json"));
    EXPECT_EQ(m.state(), "read");
    EXPECT_EQ(fieldNamed(m, "registration")["value"].toString(), "N738SX");
    EXPECT_FALSE(fieldNamed(m, "registration")["missing"].toBool());
    ASSERT_GE(m.layoutIndex(), 0);
    EXPECT_EQ(m.layouts()[m.layoutIndex()].toMap()["name"].toString(), "737-800 Two-class");
    const QVariantList holds = m.holds();
    ASSERT_EQ(holds.size(), 2);
    EXPECT_EQ(holds[0].toMap()["lb"].toString(), "7,941");
    EXPECT_EQ(holds[1].toMap()["lb"].toString(), "10,059");
    EXPECT_EQ(m.refusal(), "");
}

// Review Focus 1: a field Claude couldn't read blocks Lease until Dave types it in.
TEST(AddAircraft, MissingFieldBlocksLease) {
    db::Database db = airlineWithASlot();
    FakeReviewClient claude;
    AddAircraftModel m(db, shippedAircraft(), kRatesFile, claude, day27);
    m.readImage(QByteArray("png bytes"), "image/png");
    claude.answer(200, readTestFile("airframe_read_partial.json"));
    EXPECT_TRUE(fieldNamed(m, "mlwLb")["missing"].toBool());
    EXPECT_TRUE(m.refusal().contains("Max Landing Weight")) << m.refusal().toStdString();
    m.setField("mlwLb", "146,300");
    EXPECT_FALSE(fieldNamed(m, "mlwLb")["missing"].toBool());
    EXPECT_EQ(m.refusal(), "");
}

TEST(AddAircraft, InstantDeliveryPrice) {
    db::Database db = airlineWithASlot();
    FakeReviewClient claude;
    AddAircraftModel m(db, shippedAircraft(), kRatesFile, claude, day27);
    m.readImage(QByteArray("png bytes"), "image/png");
    claude.answer(200, readTestFile("airframe_read_ok.json"));
    EXPECT_TRUE(m.deliveryLines().isEmpty());  // factory delivery: nothing to buy
    m.setDelivery("instant");
    m.setDeliveryIcao("KSAN");
    const QVariantList lines = m.deliveryLines();
    ASSERT_FALSE(lines.isEmpty());
    EXPECT_EQ(lines[0].toMap()["label"].toString(), "Delivery fee");
    const auto distance = blocktime::airportDistanceNm(db, "KBFI", "KSAN");
    ASSERT_TRUE(distance.has_value());
    const blocktime::Rates rates = blocktime::loadRates(kRatesFile, nullptr).value_or(blocktime::Rates{});
    const blocktime::Preview price =
        blocktime::instantDeliveryPrice(*shippedAircraft().type("737-800"), *distance, 146300, rates);
    EXPECT_EQ(m.cashAfterText().toStdString(),
              blocktime::formatMoneyFull(blocktime::cashCents(db) - 23'500'000 + price.totalCents));
}

TEST(AddAircraft, NetworkErrorAllowsRetry) {
    db::Database db = airlineWithASlot();
    FakeReviewClient claude;
    AddAircraftModel m(db, shippedAircraft(), kRatesFile, claude, day27);
    m.readImage(QByteArray("png bytes"), "image/png");
    claude.answer(0, "", "Host not found");
    EXPECT_EQ(m.state(), "failed");
    EXPECT_FALSE(m.readError().isEmpty());
    m.retry();
    EXPECT_EQ(claude.bodies.size(), 2u);
    EXPECT_EQ(m.state(), "reading");
}

TEST(AddAircraft, NoKeyLetsDaveType) {
    db::Database db = airlineWithASlot();
    FakeReviewClient claude;
    claude.key = false;
    AddAircraftModel m(db, shippedAircraft(), kRatesFile, claude, day27);
    m.readImage(QByteArray("png bytes"), "image/png");
    EXPECT_EQ(m.state(), "failed");
    EXPECT_TRUE(m.readError().contains("claude_api_key"));
    EXPECT_TRUE(claude.bodies.empty());
    m.setField("baseType", "BBJ2");
    m.setField("maxPassengers", "48");
    EXPECT_EQ(m.layouts()[m.layoutIndex()].toMap()["name"].toString(), "737-800 BBJ2 VIP");
}

TEST(AddAircraft, LeaseEmitsAndResets) {
    db::Database db = airlineWithASlot();
    FakeReviewClient claude;
    AddAircraftModel m(db, shippedAircraft(), kRatesFile, claude, day27);
    m.readImage(QByteArray("png bytes"), "image/png");
    claude.answer(200, readTestFile("airframe_read_ok.json"));
    QString leased;
    QObject::connect(&m, &AddAircraftModel::leased, [&](const QString& r) { leased = r; });
    EXPECT_TRUE(m.lease());
    EXPECT_EQ(leased, "N738SX");
    EXPECT_EQ(m.state(), "empty");
    bool found = false;
    for (const auto& a : blocktime::listAirplanes(db)) found = found || a.registration == "N738SX";
    EXPECT_TRUE(found);
}

// --- AircraftModel (D-241, D-245) ----------------------------------------------------------------------------------

namespace {

// Claude's answer for N814SX's SimBrief airframe with `bagWeight` lb bags.
std::string bbjAnswer(int bagWeight) {
    nlohmann::json r = {{"registration", "N814SX"}, {"fin_number", nullptr}, {"internal_id", "730097_1790536204614"},
                        {"base_type", "BBJ2"}, {"weight_units", "LBS"}, {"max_passengers", 48},
                        {"passenger_weight_lb", 190}, {"bag_weight_lb", bagWeight}, {"empty_weight_lb", 102187},
                        {"mzfw_lb", 138300}, {"mtow_lb", 174700}, {"mlw_lb", 146300}, {"max_fuel_lb", 69986},
                        {"max_cargo_lb", 17645}, {"min_runway_ft", 7000}};
    const nlohmann::json body = {{"model", "claude-opus-5-5"}, {"stop_reason", "end_turn"},
                                 {"content", {{{"type", "text"}, {"text", r.dump()}}}}};
    return body.dump();
}

QString rowValue(const QVariantMap& card, const QString& label) {
    for (const QVariant& row : card["rows"].toList()) {
        if (row.toMap()["label"].toString() == label) return row.toMap()["value"].toString();
    }
    return {};
}

}  // namespace

TEST(Aircraft, ShowsCardsAndRefund) {
    db::Database db = test::testAirline();
    FakeReviewClient claude;
    AircraftModel m(db, shippedAircraft(), claude, day27);
    ASSERT_TRUE(m.open("N814SX"));
    EXPECT_EQ(m.registration(), "N814SX");
    EXPECT_EQ(m.title(), "737-800 BBJ2 VIP");
    EXPECT_EQ(m.leaseMonthText(), "6 of 30 days");
    EXPECT_TRUE(m.returnText().contains("$120,000")) << m.returnText().toStdString();
    EXPECT_TRUE(m.returnText().contains("24 days")) << m.returnText().toStdString();
    EXPECT_EQ(rowValue(m.weightsCard(), "MLW"), "146,300 lb");
    EXPECT_EQ(rowValue(m.simbriefCard(), "Max payload"), "36,113 lb");
    EXPECT_EQ(rowValue(m.recordCard(), "Refund if returned today"), "$120,000");
    EXPECT_TRUE(m.canReturn());
    EXPECT_FALSE(m.open("N000XX"));
}

TEST(Aircraft, NoScreenshotYet) {
    db::Database db = test::testAirline();
    ASSERT_TRUE(db.exec("DELETE FROM airplane_simbrief"));
    FakeReviewClient claude;
    AircraftModel m(db, shippedAircraft(), claude, day27);
    ASSERT_TRUE(m.open("N814SX"));
    EXPECT_EQ(m.weightsCard()["source"].toString(), "No screenshot yet");
    EXPECT_EQ(rowValue(m.weightsCard(), "MLW"), "146,300 lb");  // the limits it has
    EXPECT_EQ(rowValue(m.weightsCard(), "MTOW"), QString::fromUtf8("—"));
}

TEST(Aircraft, RereadShowsChangesThenSaves) {
    db::Database db = test::testAirline();
    FakeReviewClient claude;
    AircraftModel m(db, shippedAircraft(), claude, day27);
    ASSERT_TRUE(m.open("N814SX"));
    m.readImage(QByteArray("png bytes"), "image/png");
    EXPECT_EQ(m.rereadState(), "reading");
    claude.answer(200, bbjAnswer(80));
    EXPECT_EQ(m.rereadState(), "read");
    const QVariantList changes = m.rereadChanges();
    ASSERT_EQ(changes.size(), 1);
    EXPECT_EQ(changes[0].toMap()["label"].toString(), "Bag weight");
    EXPECT_EQ(changes[0].toMap()["after"].toString(), "80 lb");
    EXPECT_EQ(m.rereadRefusal(), "");
    EXPECT_TRUE(m.saveReread());
    EXPECT_EQ(m.rereadState(), "idle");
    EXPECT_EQ(rowValue(m.weightsCard(), "Passenger / bag"), "190 / 80 lb");
}

TEST(Aircraft, ReturnDisabledWithPlannedFlight) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakeReviewClient claude;
    AircraftModel m(db, shippedAircraft(), claude, day27);
    ASSERT_TRUE(m.open("N814SX"));
    EXPECT_FALSE(m.canReturn());
    EXPECT_FALSE(m.canReread());
}

TEST(Aircraft, ReturnPlaneEmitsAndFreesTheSlot) {
    db::Database db = test::testAirline();
    FakeReviewClient claude;
    AircraftModel m(db, shippedAircraft(), claude, day27);
    ASSERT_TRUE(m.open("N738BT"));
    int returned = 0;
    QObject::connect(&m, &AircraftModel::returned, [&] { ++returned; });
    EXPECT_TRUE(m.returnPlane());
    EXPECT_EQ(returned, 1);
    for (const auto& a : blocktime::listAirplanes(db)) {
        if (a.registration == "N738BT") EXPECT_TRUE(a.returnedOn.has_value());
    }
}

// Final review: a return that fails (here the plane was returned unpaid behind the screen's back) says why.
TEST(Aircraft, ReturnFailureIsShown) {
    db::Database db = test::testAirline();
    FakeReviewClient claude;
    AircraftModel m(db, shippedAircraft(), claude, day27);
    ASSERT_TRUE(m.open("N738BT"));
    ASSERT_TRUE(db.exec("UPDATE airplane SET returned_on = '2026-11-27', return_reason = 'unpaid'"
                        " WHERE registration = 'N738BT'"));
    EXPECT_FALSE(m.returnPlane());
    EXPECT_FALSE(m.returnError().isEmpty());
}

// --- Dispatch on Your Flight (milestone 16) ---

namespace {

class Dispatch : public TempLog {
protected:
    std::unique_ptr<YourFlightModel> your() {
        return std::make_unique<YourFlightModel>(db_, "XLY", planner_, bridge_, target_, dir_, ratesFile_);
    }
    std::unique_ptr<DispatchModel> dispatchOn(YourFlightModel& y) {
        return std::make_unique<DispatchModel>(db_, "XLY", ratesFile_, planner_, bridge_, events_, y);
    }

    // SimBrief's OFP for the planned flight as it is: its loads, flight number and the time now.
    QByteArray ofpFor(int rampFuelLb = 14200, int passengersDelta = 0) {
        const auto v = currentPlannedFlight(db_);
        EXPECT_TRUE(v.has_value());
        nlohmann::json ofp = nlohmann::json::parse(dataFile("ofp_v2_min.json"));
        int people = 0;
        for (const CabinLoad& c : v->cabins) people += c.passengers;
        ofp["params"]["time_generated"] =
            std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
        ofp["origin"]["icao_code"] = v->origin;
        ofp["destination"]["icao_code"] = v->destination;
        ofp["aircraft"]["internal_id"] = v->airframeId;
        ofp["general"]["flight_number"] = v->flightNumber;
        ofp["weights"]["pax_count"] = std::to_string(people + passengersDelta);
        ofp["weights"]["freight_added"] = std::to_string(v->cargoLb);
        ofp["fuel"]["plan_ramp"] = std::to_string(rampFuelLb);
        return QByteArray::fromStdString(ofp.dump());
    }

    // N814SX (VIP) dispatched to KSAN and its plan imported.
    void dispatchAndImport(DispatchModel& d) {
        ASSERT_TRUE(d.openForAirplane("N814SX"));
        d.setDestination("KSAN");
        ASSERT_TRUE(d.canDispatch()) << d.destinationMessage().toStdString();
        d.dispatch();
        planner_.ofpBody = ofpFor();
        d.importPlan();
        ASSERT_TRUE(d.imported()) << d.message().toStdString();
    }

    std::int64_t count(const char* sql) {
        db::Statement q(db_, sql);
        return q.next() ? q.integer(0) : -1;
    }

    static Cents line(const Preview& p, const char* category) {
        for (const auto* lines : {&p.income, &p.costs}) {
            for (const MoneyLine& m : *lines) {
                if (m.category == category) return m.cents;
            }
        }
        return 0;
    }

    db::Database db_ = deliveredAirline();
    FakePlanner planner_;
    RecorderBridge bridge_;
    FlightTargetSlot target_;
    AppEventQueue events_;
    std::filesystem::path ratesFile_ = kRatesFile;
};

}  // namespace

TEST_F(Dispatch, FormToReportToAccept) {
    auto y = your();
    auto d = dispatchOn(*y);
    ASSERT_TRUE(d->openForAirplane("N814SX"));
    EXPECT_EQ(y->stageText(), "form");
    d->setDestination("KSAN");
    d->dispatch();
    EXPECT_EQ(y->stageText(), "dispatched");
    EXPECT_FALSE(d->imported());
    const QVariantList before = d->boardRows();
    ASSERT_FALSE(before.isEmpty());
    for (const QVariant& r : before) {
        const QString text = r.toMap().value("text").toString();
        EXPECT_EQ(text.size(), 20);
        EXPECT_TRUE(text.endsWith(' ')) << text.toStdString();  // blank value tiles
    }
    EXPECT_TRUE(d->ring().value("slices").toList().isEmpty());

    planner_.ofpBody = ofpFor();
    d->importPlan();
    EXPECT_EQ(y->stageText(), "imported") << d->message().toStdString();
    EXPECT_FALSE(d->ring().value("loss").toBool());
    EXPECT_EQ(d->boardRows().first().toMap().value("id").toString(), "vip_fares");
    EXPECT_EQ(d->centerLabel(), "Profit");
    for (const QVariant& r : d->boardRows()) {  // the first import flips every row
        const QVariantMap row = r.toMap();
        EXPECT_TRUE(row.value("changed").toBool()) << row.value("id").toString().toStdString();
        const QString text = row.value("text").toString();
        const int valueStart = row.value("valueStart").toInt();
        EXPECT_TRUE(text.mid(valueStart).startsWith('+') || text.mid(valueStart).startsWith('-') ||
                    text.mid(valueStart).startsWith('$'))
            << text.toStdString();
        EXPECT_EQ(text.at(valueStart - 1), ' ');
    }
    QMap<QString, int> shades;  // each family's slices numbered 0, 1, 2...
    for (const QVariant& s : d->ring().value("slices").toList()) {
        const QVariantMap slice = s.toMap();
        EXPECT_EQ(slice.value("shade").toInt(), shades[slice.value("family").toString()]++);
    }
    EXPECT_EQ(d->seatSegments(), 48);
    EXPECT_EQ(d->seatSegmentsFilled(), d->seatsFilled());

    d->accept();
    EXPECT_EQ(y->stageText(), "accepted");
    EXPECT_TRUE(plannedAcceptance(db_, y->view()->id).has_value());
    const auto queued = events_.drain();
    ASSERT_EQ(queued.size(), 1u);
    EXPECT_EQ(queued[0].type, "flight_accepted");
    EXPECT_EQ(queued[0].data["at_block_out"], false);
    ASSERT_TRUE(target_.get().has_value());
    EXPECT_TRUE(target_.get()->accepted);
}

// Review Focus 1: a re-import after Accept that the check refuses changes nothing.
TEST_F(Dispatch, RefusedImportKeepsEverything) {
    auto y = your();
    auto d = dispatchOn(*y);
    dispatchAndImport(*d);
    d->accept();
    const auto acceptedBefore = plannedAcceptance(db_, y->view()->id);
    const std::string requestBefore = y->view()->plan->requestId;
    planner_.ofpBody = ofpFor(14200, -1);  // one passenger short
    d->importPlan();
    EXPECT_TRUE(d->message().startsWith("SimBrief has ")) << d->message().toStdString();
    EXPECT_TRUE(d->message().endsWith("Open SimBrief to generate it again with this flight's loads."));
    EXPECT_EQ(y->view()->plan->requestId, requestBefore);
    const auto acceptedAfter = plannedAcceptance(db_, y->view()->id);
    ASSERT_TRUE(acceptedAfter.has_value());
    EXPECT_EQ(acceptedAfter->acceptedAt, acceptedBefore->acceptedAt);
    EXPECT_EQ(acceptedAfter->money.totalCents, acceptedBefore->money.totalCents);
}

TEST_F(Dispatch, ReimportAfterAcceptReplacesQuietly) {
    auto y = your();
    auto d = dispatchOn(*y);
    dispatchAndImport(*d);
    d->accept();
    const Cents fuelBefore = line(plannedAcceptance(db_, y->view()->id)->money, "fuel");
    planner_.ofpBody = ofpFor(15200);  // 1,000 lb more ramp fuel
    d->importPlan();
    EXPECT_EQ(d->message(), QString()) << d->message().toStdString();
    EXPECT_EQ(y->stageText(), "accepted");
    const Cents fuelAfter = line(plannedAcceptance(db_, y->view()->id)->money, "fuel");
    EXPECT_LT(fuelAfter, fuelBefore);  // a bigger cost
    QStringList changed;
    for (const QVariant& r : d->boardRows()) {
        if (r.toMap().value("changed").toBool()) changed << r.toMap().value("id").toString();
    }
    EXPECT_EQ(changed, (QStringList{"fuel", "profit"}));
}

// Review Focus 2: after a restart the report is the stored breakdown, not today's rates.yaml.
TEST_F(Dispatch, RestartShowsStoredBreakdown) {
    Cents pilotsAccepted = 0;
    {
        auto y = your();
        auto d = dispatchOn(*y);
        dispatchAndImport(*d);
        d->accept();
        pilotsAccepted = line(*d->preview(), "pilots");
    }
    // rates.yaml edited: the VIP pilot rate doubled.
    std::ifstream in(kRatesFile, std::ios::binary);
    std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    const auto at = text.find("  pilot_per_hour: 250");
    ASSERT_NE(at, std::string::npos);
    text.replace(at, 21, "  pilot_per_hour: 500");
    ratesFile_ = dir_ / "rates.yaml";
    std::ofstream(ratesFile_, std::ios::binary) << text;

    auto y = your();
    auto d = dispatchOn(*y);
    EXPECT_EQ(y->stageText(), "accepted");
    EXPECT_NE(pilotsAccepted, 0);
    EXPECT_EQ(line(*d->preview(), "pilots"), pilotsAccepted);
    ASSERT_TRUE(target_.get().has_value());
    EXPECT_TRUE(target_.get()->accepted);
}

TEST_F(Dispatch, LockedAfterBlockOut) {
    auto y = your();
    auto d = dispatchOn(*y);
    dispatchAndImport(*d);
    d->accept();
    BlockOut out;
    out.flightId = "2026-11-27T10:00:00.000Z";
    bridge_.applyBlockEvent(out);
    EXPECT_TRUE(d->locked());
    const int fetches = planner_.ofpFetches;
    d->importPlan();
    EXPECT_EQ(planner_.ofpFetches, fetches);
}

// Review Focus 5: imported but not accepted, block out accepts it.
TEST_F(Dispatch, BlockOutAccepts) {
    auto y = your();
    auto d = dispatchOn(*y);
    dispatchAndImport(*d);
    const std::int64_t id = y->view()->id;
    BlockOut out;
    out.flightId = "2026-11-27T10:00:00.000Z";
    bridge_.applyBlockEvent(out);
    const auto accepted = plannedAcceptance(db_, id);
    ASSERT_TRUE(accepted.has_value());
    EXPECT_TRUE(accepted->atBlockOut);
    EXPECT_TRUE(events_.drain().empty());  // the recorder wrote the marker, not the app
}

TEST_F(Dispatch, ChangeFlightKeepsLoads) {
    auto y = your();
    auto d = dispatchOn(*y);
    dispatchAndImport(*d);
    const std::vector<CabinLoad> cabins = y->view()->cabins;
    d->changeFlight();
    EXPECT_EQ(y->stageText(), "form");
    EXPECT_TRUE(d->changing());
    d->setFlightNumber("815");
    d->saveChange();
    EXPECT_EQ(y->stageText(), "dispatched");
    EXPECT_FALSE(d->imported());
    EXPECT_EQ(y->view()->flightNumber, "815");
    ASSERT_EQ(y->view()->cabins.size(), cabins.size());
    for (std::size_t i = 0; i < cabins.size(); ++i) EXPECT_EQ(y->view()->cabins[i].passengers, cabins[i].passengers);
}

TEST_F(Dispatch, NoChangeAfterAccept) {
    auto y = your();
    auto d = dispatchOn(*y);
    dispatchAndImport(*d);
    d->accept();
    d->changeFlight();
    EXPECT_FALSE(d->changing());
    EXPECT_EQ(y->stageText(), "accepted");
}

TEST_F(Dispatch, OpenSimbriefSendsTheLoadsAndExtraFuel) {
    auto y = your();
    auto d = dispatchOn(*y);
    ASSERT_TRUE(d->openForAirplane("N814SX"));
    d->setDestination("KSAN");
    d->setAdditionalFuel("1500");
    d->dispatch();
    d->openSimbrief();
    ASSERT_EQ(planner_.opened.size(), 1);
    EXPECT_TRUE(planner_.opened[0].contains("&addedfuel=1.5&addedfuel_units=wgt")) << planner_.opened[0].toStdString();
    EXPECT_TRUE(planner_.opened[0].contains("&origrwy=14R"));
    EXPECT_EQ(d->extraFuelBadge(), "+1,500 lb extra fuel");
}

TEST_F(Dispatch, CancelGoesBackToEmpty) {
    auto y = your();
    auto d = dispatchOn(*y);
    dispatchAndImport(*d);
    d->accept();
    d->cancelFlight();
    EXPECT_EQ(y->stageText(), "empty");
    EXPECT_EQ(count("SELECT count(*) FROM accepted_breakdown"), 0);
    EXPECT_FALSE(target_.get().has_value());
}

// Review Focus 4: a delivery flight's report is a loss: no fares row, the ring is the costs, "Loss" in the centre.
TEST_F(Dispatch, DeliveryIsALoss) {
    ASSERT_TRUE(db_.exec("UPDATE airplane SET awaiting_delivery = 1 WHERE registration = 'N814SX'"));
    auto y = your();
    auto d = dispatchOn(*y);
    dispatchAndImport(*d);
    for (const QVariant& r : d->boardRows()) EXPECT_NE(r.toMap().value("tone").toString(), "income");
    EXPECT_EQ(d->boardRows().last().toMap().value("tone").toString(), "loss");
    EXPECT_TRUE(d->ring().value("loss").toBool());
    EXPECT_EQ(d->centerLabel(), "Loss");
    EXPECT_EQ(d->centerTone(), "loss");
}

TEST_F(Dispatch, SelectShowsTheWorking) {
    auto y = your();
    auto d = dispatchOn(*y);
    dispatchAndImport(*d);
    d->select("fuel");
    EXPECT_EQ(d->selectedId(), "fuel");
    const QStringList lines = d->selection().value("lines").toStringList();
    ASSERT_FALSE(lines.isEmpty());
    EXPECT_TRUE(lines.first().endsWith("lb block fuel")) << lines.first().toStdString();
    EXPECT_EQ(d->centerLabel(), "Fuel");
    d->select("fuel");  // again: cleared
    EXPECT_EQ(d->selectedId(), "");
}
