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
#include <QRegularExpression>
#include <thread>

#include <gtest/gtest.h>

#include "app_event_queue.h"
#include "event_feed_model.h"
#include "flight_watcher.h"
#include "fleet_table_model.h"
#include "ledger_model.h"
#include "flight_list_model.h"
#include "new_flight_model.h"
#include "flight_settlement.h"
#include "planned_flight.h"
#include "planned_flight_model.h"
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

// --- FleetTableModel ----------------------------------------------------------------------------------------------

namespace {

AirplaneView plane(const std::string& registration, const std::string& airframe, int fuel) {
    AirplaneView a;
    a.registration = registration;
    a.typeCode = "737-800";
    a.airframeName = airframe;
    a.fleet = "Main";
    a.locationIcao = "KSAN";
    a.fuelOnBoardLb = fuel;
    return a;
}

std::vector<AirplaneView> sampleFleet() {
    return {plane("N880SX", "VIP", 12128), plane("N601SX", "all-economy", 950), plane("N818SX", "Two-class", 3000),
            plane("N840SX", "Freighter", 0)};
}

QString cell(const FleetTableModel& m, int row, int column) {
    return m.data(m.index(row, column), Qt::DisplayRole).toString();
}

std::vector<QString> column(const FleetTableModel& m, int col) {
    std::vector<QString> out;
    for (int r = 0; r < m.rowCount(); ++r) out.push_back(cell(m, r, col));
    return out;
}

constexpr Date day(int y, unsigned m, unsigned d) {
    return Date{std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}};
}

}  // namespace

TEST(Fleet, DefaultsToRegistrationAscending) {
    FleetTableModel m(sampleFleet(), {});
    EXPECT_EQ(m.rowCount(), 4);
    EXPECT_EQ(m.columnCount(), FleetTableModel::ColumnCount);
    EXPECT_EQ(column(m, FleetTableModel::Registration),
              (std::vector<QString>{"N601SX", "N818SX", "N840SX", "N880SX"}));
    EXPECT_EQ(m.headerData(FleetTableModel::Airframe, Qt::Horizontal, Qt::DisplayRole).toString(), "Airframe");
    EXPECT_EQ(m.headerData(FleetTableModel::Fuel, Qt::Horizontal, Qt::DisplayRole).toString(), "Fuel");
    EXPECT_FALSE(m.headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
}

TEST(Fleet, TextSortIsCaseInsensitive) {
    FleetTableModel m(sampleFleet(), {});
    int sortChanges = 0;
    QObject::connect(&m, &FleetTableModel::sortChanged, [&] { ++sortChanges; });
    m.sort(FleetTableModel::Airframe, Qt::AscendingOrder);
    // Case-sensitive order would put "all-economy" last.
    EXPECT_EQ(column(m, FleetTableModel::Airframe),
              (std::vector<QString>{"all-economy", "Freighter", "Two-class", "VIP"}));
    m.sort(FleetTableModel::Airframe, Qt::DescendingOrder);
    EXPECT_EQ(column(m, FleetTableModel::Airframe),
              (std::vector<QString>{"VIP", "Two-class", "Freighter", "all-economy"}));
    EXPECT_EQ(sortChanges, 2);
}

TEST(Fleet, IgnoresABadSortColumn) {
    FleetTableModel m(sampleFleet(), {});
    m.sort(-1);
    m.sort(FleetTableModel::ColumnCount);
    EXPECT_EQ(m.sortColumn(), FleetTableModel::Registration);
    EXPECT_EQ(cell(m, 0, FleetTableModel::Registration), "N601SX");
}

TEST(Fleet, FuelSortsByNumberAndShowsPounds) {
    FleetTableModel m(sampleFleet(), {});
    m.sort(FleetTableModel::Fuel, Qt::AscendingOrder);
    // Text order would put "12,128 lb" before "3,000 lb".
    EXPECT_EQ(column(m, FleetTableModel::Fuel), (std::vector<QString>{"0 lb", "950 lb", "3,000 lb", "12,128 lb"}));
}

TEST(Fleet, StatusText) {
    std::vector<AirplaneView> rows = sampleFleet();
    rows[0].awaitingDelivery = true;                // N880SX
    rows[1].returnedOn = day(2027, 1, 22);          // N601SX
    rows[1].returnReason = "unpaid";
    FleetTableModel m(rows, {});
    EXPECT_EQ(column(m, FleetTableModel::Status),
              (std::vector<QString>{"Returned 22 Jan 2027", "Parked", "Parked", "Awaiting delivery"}));
}

TEST(Fleet, FlightsAndHoursColumns) {
    std::vector<AirplaneView> rows = sampleFleet();
    rows[0].flights = 3;  // N880SX
    rows[0].blockHours = 7.25;
    FleetTableModel m(rows, {});
    EXPECT_EQ(m.headerData(FleetTableModel::Flights, Qt::Horizontal, Qt::DisplayRole).toString(), "Flights");
    EXPECT_EQ(m.headerData(FleetTableModel::Hours, Qt::Horizontal, Qt::DisplayRole).toString(), "Hours");
    EXPECT_EQ(column(m, FleetTableModel::Flights), (std::vector<QString>{"0", "0", "0", "3"}));
    EXPECT_EQ(column(m, FleetTableModel::Hours), (std::vector<QString>{"0.0", "0.0", "0.0", "7.3"}));
    m.sort(FleetTableModel::Hours, Qt::DescendingOrder);
    EXPECT_EQ(cell(m, 0, FleetTableModel::Registration), "N880SX");
}

TEST(Fleet, SummaryText) {
    FleetTableModel m(sampleFleet(), {}, "3.2 h flown");
    EXPECT_EQ(m.summaryText(), "3.2 h flown");
    int changes = 0;
    QObject::connect(&m, &FleetTableModel::summaryTextChanged, [&] { ++changes; });
    m.reset(sampleFleet(), {}, "3.2 h flown");
    EXPECT_EQ(changes, 0);
    m.reset(sampleFleet(), {}, "4.0 h flown");
    EXPECT_EQ(m.summaryText(), "4.0 h flown");
    EXPECT_EQ(changes, 1);
    EXPECT_EQ(FleetTableModel::fleetSummary(3.21, 1), "3.2 h flown · 1 of 2 slots in use · slot 3 at 10 h");
    EXPECT_EQ(FleetTableModel::fleetSummary(12, 1), "12.0 h flown · 1 of 3 slots in use · slot 4 at 30 h");
}

TEST(Fleet, ResetReplacesRowsAndNotices) {
    FleetTableModel m(sampleFleet(), {"No airport data yet."});
    EXPECT_EQ(m.notices(), QStringList{"No airport data yet."});
    int noticeChanges = 0;
    QObject::connect(&m, &FleetTableModel::noticesChanged, [&] { ++noticeChanges; });
    m.sort(FleetTableModel::Fuel, Qt::DescendingOrder);
    m.reset({plane("N1", "VIP", 5), plane("N2", "VIP", 7)}, {});
    EXPECT_EQ(m.rowCount(), 2);
    EXPECT_EQ(cell(m, 0, FleetTableModel::Registration), "N2");  // the sort is kept
    EXPECT_TRUE(m.notices().isEmpty());
    EXPECT_EQ(noticeChanges, 1);
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

TEST(PlannedFlightModel, ImportStoresMatchingPlan) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    PlannedFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
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

TEST(PlannedFlightModel, ImportRefusesAStalePlan) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:20:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    PlannedFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.importPlan();
    EXPECT_EQ(m.state(), "waitingForPlan");
    EXPECT_TRUE(m.message().startsWith("The latest SimBrief plan was generated at 14:02Z")) << m.message().toStdString();
    EXPECT_FALSE(currentPlannedFlight(db)->plan.has_value());
}

TEST(PlannedFlightModel, ImportShowsSimbriefError) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    planner.ofpBody = R"({"fetch":{"userid":"","static_id":"","status":"Error: Unknown UserID","time":"0"}})";
    RecorderBridge bridge;
    PlannedFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.importPlan();
    EXPECT_TRUE(m.message().contains("Error: Unknown UserID")) << m.message().toStdString();
    EXPECT_EQ(m.state(), "waitingForPlan");
}

TEST(PlannedFlightModel, ImportShowsNetworkError) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    planner.ofpBody.clear();
    planner.ofpError = "Add simbrief_user_id to settings.json";
    RecorderBridge bridge;
    PlannedFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.importPlan();
    EXPECT_EQ(m.message(), "Add simbrief_user_id to settings.json");
}

TEST(PlannedFlightModel, CancelClears) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    PlannedFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    int changed = 0;
    QObject::connect(&m, &PlannedFlightModel::flightChanged, [&] { ++changed; });
    m.cancelFlight();
    EXPECT_FALSE(m.hasFlight());
    EXPECT_EQ(changed, 1);
    EXPECT_FALSE(currentPlannedFlight(db).has_value());
}

TEST(PlannedFlightModel, StateFollowsConnection) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    PlannedFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.importPlan();
    ASSERT_EQ(m.state(), "waitingForSim");
    bridge.applyConnection(ConnectionState::Connected);
    EXPECT_EQ(m.state(), "simConnected");
    EXPECT_EQ(m.stateText(), "Sim connected");
}

TEST(PlannedFlightModel, OpenDispatchPageRebuildsTheUrl) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    PlannedFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
    m.openDispatchPage();
    ASSERT_EQ(planner.opened.size(), 1);
    EXPECT_TRUE(planner.opened[0].contains("orig=KBFI&dest=KSAN&airline=XLY&fltnum=8")) << planner.opened[0].toStdString();
    EXPECT_TRUE(planner.opened[0].contains("&pax=46&")) << planner.opened[0].toStdString();
}

TEST(Fleet, RegistrationAtRow) {
    FleetTableModel m({plane("N814SX", "737-800 BBJ2 VIP", 0), plane("N738BT", "737-800 Two-class", 0)}, {});
    EXPECT_EQ(m.registrationAt(0), "N738BT");  // sorted by registration
    EXPECT_EQ(m.registrationAt(1), "N814SX");
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
    }

    std::unique_ptr<PlannedFlightModel> model() {
        return std::make_unique<PlannedFlightModel>(db_, "XLY", planner_, bridge_, target_, dir_, kRatesFile);
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
    QObject::connect(m.get(), &PlannedFlightModel::settled, [&] { ++settled; });
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
    QObject::connect(m.get(), &PlannedFlightModel::settled, [&] { ++settled; });
    m->checkFlown();
    EXPECT_EQ(settled, 1);
    EXPECT_EQ(m->summaryFlightId(), kLiveOut);
    m->checkFlown();  // nothing left to settle
    EXPECT_EQ(settled, 1);
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
    EXPECT_EQ(m->state(), "waitingForPlan");
    EXPECT_TRUE(target_.get().has_value());
}

TEST_F(LiveFlight, TurnaroundAsksForNewFlight) {
    write({liveOut(plannedId_), liveIn()});
    auto m = model();
    m->checkFlown();
    QStringList asked;
    QObject::connect(m.get(), &PlannedFlightModel::turnaroundRequested, [&](const QString& r) { asked << r; });
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
    PlannedFlightModel m(db_, "XLY", planner_, bridge_, target_, dir_, dir_ / "no-rates.yaml");
    int settled = 0;
    QObject::connect(&m, &PlannedFlightModel::settled, [&] { ++settled; });
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
    EXPECT_EQ(lines[0].toMap()["text"].toString(), "Refuelling");
    EXPECT_TRUE(lines[0].toMap()["checks"].toList().isEmpty());
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

TEST(PlannedFlightModel, LoadInTextNamesTheSpot) {
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
    PlannedFlightModel m(db, "XLY", planner, bridge, g_target, kNoLog, kRatesFile);
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
    EXPECT_EQ(landing[1], QString::fromUtf8("2.7 m off the centreline, 0.6° off the heading (information)"));
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
