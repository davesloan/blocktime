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
#include <string>
#include <tuple>
#include <vector>

#include <QRegularExpression>

#include <gtest/gtest.h>

#include "event_feed_model.h"
#include "fleet_table_model.h"
#include "flight_list_model.h"
#include "recorder_bridge.h"

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
    const QGeoPath path = m.track(0);
    ASSERT_EQ(path.size(), static_cast<qsizetype>(m.records()[0].track.size()));
    EXPECT_DOUBLE_EQ(path.coordinateAt(0).latitude(), 32.73);
    EXPECT_TRUE(m.track(-1).isEmpty());
    EXPECT_TRUE(m.track(1).isEmpty());
}

TEST_F(TempLog, ReloadPicksUpAFlightThatJustEnded) {
    write({blockOut(kOut, "N818SX")});
    FlightListModel m(dir_);
    int countChanges = 0, resets = 0;
    QObject::connect(&m, &FlightListModel::countChanged, [&] { ++countChanges; });
    QObject::connect(&m, &QAbstractItemModel::modelReset, [&] { ++resets; });
    EXPECT_EQ(m.data(m.index(0), FlightListModel::StatusRole).toString(), "open");

    write({blockIn(kOut, "2026-11-22T21:55:15.475Z", 1612)}, true);
    m.reload();
    EXPECT_EQ(m.count(), 1);
    EXPECT_EQ(m.data(m.index(0), FlightListModel::StatusRole).toString(), "block_in");
    EXPECT_EQ(resets, 1);
    EXPECT_EQ(countChanges, 0);  // same number of flights

    write({blockOut("2026-11-22T23:00:00.000Z", "N818SX")}, true);
    m.reload();
    EXPECT_EQ(m.count(), 2);
    EXPECT_EQ(countChanges, 1);
}

// --- FleetTableModel ----------------------------------------------------------------------------------------------

namespace {

std::vector<Airframe> sampleFleet() {
    return {{"N818SX", "737-800", "Two-class", "KSAN", "Parked"},
            {"N601SX", "737-600", "all-economy", "KBFI", "Parked"},
            {"N880SX", "737-800 BBJ2", "VIP", "KLAS", "Maintenance"},
            {"N840SX", "737-800 BCF", "Freighter", "KONT", "Parked"}};
}

QString cell(const FleetTableModel& m, int row, int column) {
    return m.data(m.index(row, column), Qt::DisplayRole).toString();
}

std::vector<QString> column(const FleetTableModel& m, int col) {
    std::vector<QString> out;
    for (int r = 0; r < m.rowCount(); ++r) out.push_back(cell(m, r, col));
    return out;
}

}  // namespace

TEST_F(TempLog, FleetDefaultsToRegistrationAscending) {
    FlightListModel flights(dir_);
    FleetTableModel m(sampleFleet(), "", flights);
    EXPECT_EQ(m.rowCount(), 4);
    EXPECT_EQ(m.columnCount(), FleetTableModel::ColumnCount);
    EXPECT_EQ(column(m, FleetTableModel::Registration),
              (std::vector<QString>{"N601SX", "N818SX", "N840SX", "N880SX"}));
    EXPECT_EQ(m.headerData(FleetTableModel::BlockHours, Qt::Horizontal, Qt::DisplayRole).toString(), "Block hours");
    EXPECT_FALSE(m.headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());
}

TEST_F(TempLog, FleetCountsBlockInFlightsPerTail) {
    // N818SX: 10 flights of 1 h; N601SX: 9 of 2 h; N840SX: one interrupted flight, which doesn't count.
    std::vector<std::tuple<std::string, int, int>> list;
    for (int h = 0; h < 10; ++h) list.emplace_back("N818SX", h, 3600);
    flights(list);
    std::vector<std::string> more;
    for (int h = 10; h < 19; ++h) {
        const std::string id = std::format("2026-11-22T{:02}:00:00.000Z", h);
        more.push_back(blockOut(id, "N601SX"));
        more.push_back(blockIn(id, std::format("2026-11-22T{:02}:30:00.000Z", h), 7200));
    }
    more.push_back(blockOut("2026-11-22T20:00:00.000Z", "N840SX"));
    more.push_back(interrupted("2026-11-22T20:00:00.000Z", "2026-11-22T20:05:00.000Z", "sim_stopped"));
    write(more, true);

    FlightListModel fl(dir_);
    FleetTableModel m(sampleFleet(), "", fl);
    // Rows in registration order: N601SX, N818SX, N840SX, N880SX.
    EXPECT_EQ(column(m, FleetTableModel::Flights), (std::vector<QString>{"9", "10", "0", "0"}));
    EXPECT_EQ(column(m, FleetTableModel::BlockHours), (std::vector<QString>{"18.0", "10.0", "0.0", "0.0"}));

    // Numbers sort by value: "10" before "9" would be text order.
    m.sort(FleetTableModel::Flights, Qt::DescendingOrder);
    EXPECT_EQ(column(m, FleetTableModel::Registration),
              (std::vector<QString>{"N818SX", "N601SX", "N840SX", "N880SX"}));  // ties by registration, ascending
    m.sort(FleetTableModel::BlockHours, Qt::AscendingOrder);
    EXPECT_EQ(column(m, FleetTableModel::Registration),
              (std::vector<QString>{"N840SX", "N880SX", "N818SX", "N601SX"}));
    EXPECT_EQ(m.sortColumn(), FleetTableModel::BlockHours);
    EXPECT_EQ(m.sortOrder(), static_cast<int>(Qt::AscendingOrder));
}

TEST_F(TempLog, FleetTextSortIsCaseInsensitive) {
    FlightListModel flights(dir_);
    FleetTableModel m(sampleFleet(), "", flights);
    int sortChanges = 0;
    QObject::connect(&m, &FleetTableModel::sortChanged, [&] { ++sortChanges; });
    m.sort(FleetTableModel::Configuration, Qt::AscendingOrder);
    // Case-sensitive order would put "all-economy" last.
    EXPECT_EQ(column(m, FleetTableModel::Configuration),
              (std::vector<QString>{"all-economy", "Freighter", "Two-class", "VIP"}));
    m.sort(FleetTableModel::Configuration, Qt::DescendingOrder);
    EXPECT_EQ(column(m, FleetTableModel::Configuration),
              (std::vector<QString>{"VIP", "Two-class", "Freighter", "all-economy"}));
    EXPECT_EQ(sortChanges, 2);
}

TEST_F(TempLog, FleetIgnoresABadSortColumn) {
    FlightListModel flights(dir_);
    FleetTableModel m(sampleFleet(), "", flights);
    m.sort(-1);
    m.sort(FleetTableModel::ColumnCount);
    EXPECT_EQ(m.sortColumn(), FleetTableModel::Registration);
    EXPECT_EQ(cell(m, 0, FleetTableModel::Registration), "N601SX");
}

TEST_F(TempLog, FleetRecountsWhenFlightsReload) {
    flights({{"N818SX", 1, 1800}});
    FlightListModel fl(dir_);
    FleetTableModel m(sampleFleet(), "", fl);
    EXPECT_EQ(cell(m, 1, FleetTableModel::Flights), "1");  // N818SX is row 1
    write({blockOut("2026-11-22T05:00:00.000Z", "N818SX"),
           blockIn("2026-11-22T05:00:00.000Z", "2026-11-22T05:30:00.000Z", 1800)},
          true);
    fl.reload();
    EXPECT_EQ(cell(m, 1, FleetTableModel::Flights), "2");
    EXPECT_EQ(cell(m, 1, FleetTableModel::BlockHours), "1.0");
}

TEST_F(TempLog, FleetCarriesTheStoreError) {
    FlightListModel flights(dir_);
    FleetTableModel m({}, "unable to open database file", flights);
    EXPECT_EQ(m.rowCount(), 0);
    EXPECT_EQ(m.error(), "unable to open database file");
}
