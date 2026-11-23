// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Tests for the Qt side: the event feed, RecorderBridge and the Flights and Fleet models. No QML and no window; the
// models are plain QObjects, so no QGuiApplication is needed.

#include <chrono>
#include <cstring>
#include <filesystem>
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

#include "event_feed_model.h"
#include "fleet_table_model.h"
#include "ledger_model.h"
#include "flight_list_model.h"
#include "new_flight_model.h"
#include "planned_flight.h"
#include "planned_flight_model.h"
#include "planner.h"
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
    feed.add("info", "first");
    feed.add("block", "second");
    ASSERT_EQ(feed.rowCount(), 2);
    EXPECT_EQ(feed.data(feed.index(0), EventFeedModel::TextRole).toString(), "second");
    EXPECT_EQ(feed.data(feed.index(0), EventFeedModel::KindRole).toString(), "block");
    EXPECT_TRUE(QRegularExpression(R"(^\d\d:\d\d:\d\d$)")
                    .match(feed.data(feed.index(1), EventFeedModel::TimeRole).toString())
                    .hasMatch());
}

TEST(EventFeed, CappedAtMaxRows) {
    EventFeedModel feed;
    for (int i = 0; i < EventFeedModel::kMaxRows + 5; ++i) feed.add("info", QString::number(i));
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

// --- FlightListModel ----------------------------------------------------------------------------------------------

TEST_F(TempLog, FlightRolesFormatted) {
    write({blockOut(kOut, "N818SX"), tel("2026-11-22T21:40:00.000Z", 32.74),
           blockIn(kOut, "2026-11-22T21:55:15.475Z", 1612)});
    FlightListModel m(dir_);
    m.reloadBlocking();
    ASSERT_EQ(m.count(), 1);
    const QModelIndex i = m.index(0);
    EXPECT_EQ(m.data(i, FlightListModel::DateTextRole).toString(), "22 Nov 2026");
    EXPECT_EQ(m.data(i, FlightListModel::OutTextRole).toString(), "21:28Z");
    EXPECT_EQ(m.data(i, FlightListModel::InTextRole).toString(), "21:55Z");
    EXPECT_EQ(m.data(i, FlightListModel::BlockTextRole).toString(), "0:26");
    EXPECT_NEAR(m.data(i, FlightListModel::FuelUsedLbRole).toDouble(), 3174.6, 1e-6);
    EXPECT_EQ(m.data(i, FlightListModel::AtcIdRole).toString(), "N818SX");
    EXPECT_EQ(m.data(i, FlightListModel::StatusRole).toString(), "block_in");
    EXPECT_EQ(m.data(i, FlightListModel::StatusTextRole).toString(), "Block in");
    EXPECT_FALSE(m.data(m.index(1), FlightListModel::AtcIdRole).isValid());  // past the end
}

TEST_F(TempLog, InterruptedFlightRoles) {
    write({blockOut(kOut, "N818SX"), interrupted(kOut, "2026-11-22T21:31:50.032Z", "sim_stopped")});
    FlightListModel m(dir_);
    m.reloadBlocking();
    ASSERT_EQ(m.count(), 1);
    const QModelIndex i = m.index(0);
    EXPECT_EQ(m.data(i, FlightListModel::InTextRole).toString(), "21:31Z");
    EXPECT_EQ(m.data(i, FlightListModel::BlockTextRole).toString(), "0:03");  // from out to the interruption
    EXPECT_EQ(m.data(i, FlightListModel::FuelUsedLbRole).toDouble(), -1.0);
    EXPECT_EQ(m.data(i, FlightListModel::StatusRole).toString(), "interrupted");
    EXPECT_EQ(m.data(i, FlightListModel::StatusTextRole).toString(), QString::fromUtf8("Interrupted · sim_stopped"));
}

TEST_F(TempLog, OpenFlightRoles) {
    write({blockOut(kOut, "N818SX"), tel("2026-11-22T21:40:00.000Z", 32.74)});
    FlightListModel m(dir_);
    m.reloadBlocking();
    const QModelIndex i = m.index(0);
    EXPECT_EQ(m.data(i, FlightListModel::StatusRole).toString(), "open");
    EXPECT_EQ(m.data(i, FlightListModel::StatusTextRole).toString(), "In progress");
    EXPECT_EQ(m.data(i, FlightListModel::InTextRole).toString(), "");
    EXPECT_EQ(m.data(i, FlightListModel::BlockTextRole).toString(), QString::fromUtf8("—"));
}

TEST_F(TempLog, TrackMatchesRecord) {
    write({blockOut(kOut, "N818SX"), tel("2026-11-22T21:40:00.000Z", 32.74),
           blockIn(kOut, "2026-11-22T21:55:15.475Z", 1612)});
    FlightListModel m(dir_);
    m.reloadBlocking();
    const QGeoPath path = m.track(0);
    ASSERT_EQ(path.size(), static_cast<qsizetype>(m.records()[0].track.size()));
    EXPECT_DOUBLE_EQ(path.coordinateAt(0).latitude(), 32.73);
    EXPECT_TRUE(m.track(-1).isEmpty());
    EXPECT_TRUE(m.track(1).isEmpty());
}

TEST_F(TempLog, ReloadPicksUpAFlightThatJustEnded) {
    write({blockOut(kOut, "N818SX")});
    FlightListModel m(dir_);
    m.reloadBlocking();
    int countChanges = 0, resets = 0;
    QObject::connect(&m, &FlightListModel::countChanged, [&] { ++countChanges; });
    QObject::connect(&m, &QAbstractItemModel::modelReset, [&] { ++resets; });
    EXPECT_EQ(m.data(m.index(0), FlightListModel::StatusRole).toString(), "open");

    write({blockIn(kOut, "2026-11-22T21:55:15.475Z", 1612)}, true);
    m.reloadBlocking();
    EXPECT_EQ(m.count(), 1);
    EXPECT_EQ(m.data(m.index(0), FlightListModel::StatusRole).toString(), "block_in");
    EXPECT_EQ(resets, 1);
    EXPECT_EQ(countChanges, 0);  // same number of flights

    write({blockOut("2026-11-22T23:00:00.000Z", "N818SX")}, true);
    m.reloadBlocking();
    EXPECT_EQ(m.count(), 2);
    EXPECT_EQ(countChanges, 1);
}

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

TEST_F(TempLog, ReloadRunsOnAWorkerAndSwapsTheResultIn) {
    write({blockOut(kOut, "N818SX"), blockIn(kOut, "2026-11-22T21:55:15.475Z", 1612)});
    qtApp();
    FlightListModel m(dir_);
    EXPECT_EQ(m.count(), 0);  // construction doesn't read the log
    int loadingChanges = 0, resets = 0;
    QObject::connect(&m, &FlightListModel::loadingChanged, [&] { ++loadingChanges; });
    QObject::connect(&m, &QAbstractItemModel::modelReset, [&] { ++resets; });
    m.reload();
    EXPECT_TRUE(m.loading());
    m.reload();  // asked for again while loading: runs once more afterwards
    ASSERT_TRUE(processUntil([&] { return !m.loading() && resets == 2; }));
    EXPECT_EQ(m.count(), 1);
    EXPECT_EQ(m.data(m.index(0), FlightListModel::StatusRole).toString(), "block_in");
    EXPECT_EQ(loadingChanges, 4);  // true, false, true, false
}

TEST_F(TempLog, DestroyingTheModelWhileLoadingDoesNotWait) {
    write({blockOut(kOut, "N818SX"), blockIn(kOut, "2026-11-22T21:55:15.475Z", 1612)});
    qtApp();
    {
        FlightListModel m(dir_);
        m.reload();
    }  // stops and joins the worker; the posted result is dropped with the model
    QCoreApplication::processEvents();
    SUCCEED();
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
};

db::Database deliveredAirline() {
    db::Database db = test::testAirline();
    EXPECT_TRUE(db.exec("UPDATE airplane SET awaiting_delivery = 0 WHERE registration = 'N814SX'"));
    return db;
}

// A VIP flight N814SX KBFI-KSAN planned at `plannedAt`, written straight to the database.
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
    NewFlightModel m(db, "XLY", 4100, planner);
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
    NewFlightModel m(db, "XLY", 4100, planner);
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

TEST(NewFlightModel, DestinationMessageUpdates) {
    db::Database db = deliveredAirline();
    FakePlanner planner;
    NewFlightModel m(db, "XLY", 4100, planner);
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
    NewFlightModel m(db, "XLY", 4100, planner);
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
    PlannedFlightModel m(db, "XLY", planner, bridge);
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
    EXPECT_EQ(m.route(), "BANGR9 BANGR DCT LKV DCT OAL J65 BLH DCT JLI SEAVU2");
    EXPECT_TRUE(currentPlannedFlight(db)->plan.has_value());
}

TEST(PlannedFlightModel, ImportRefusesAStalePlan) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:20:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    PlannedFlightModel m(db, "XLY", planner, bridge);
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
    PlannedFlightModel m(db, "XLY", planner, bridge);
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
    PlannedFlightModel m(db, "XLY", planner, bridge);
    m.importPlan();
    EXPECT_EQ(m.message(), "Add simbrief_user_id to settings.json");
}

TEST(PlannedFlightModel, CancelClears) {
    db::Database db = deliveredAirline();
    planVip(db, "2026-11-23T14:00:00Z");
    FakePlanner planner;
    RecorderBridge bridge;
    PlannedFlightModel m(db, "XLY", planner, bridge);
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
    PlannedFlightModel m(db, "XLY", planner, bridge);
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
    PlannedFlightModel m(db, "XLY", planner, bridge);
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
    NewFlightModel m(db, "XLY", 4100, planner);
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
    NewFlightModel m(db, "XLY", 4100, planner);
    ASSERT_TRUE(m.open("N814SX"));
    m.setDestination("KSAN");
    m.plan();
    ASSERT_TRUE(m.open("N738BT"));
    planner.pendingRunways(planner.runways, {});
    EXPECT_TRUE(planner.opened.isEmpty());
    EXPECT_FALSE(currentPlannedFlight(db).has_value());
    EXPECT_EQ(m.error(), "");
}
