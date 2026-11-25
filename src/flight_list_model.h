// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Flights screen's list (D-111): the settled flights from the airline database, newest first, each with its money
// for the summary. Tracks still come from the event log (D-060), read on a worker thread and joined on the flight id;
// a flight whose log is gone shows a straight line between its airports.

#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include <QAbstractListModel>
#include <QGeoPath>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "debrief_measure.h"
#include "debrief_store.h"
#include "review_writer.h"
#include "airline_queries.h"
#include "flight_history.h"
#include "sqlite_db.h"

class FlightListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)  // the tracks are being read
    // The Flights screen's strip (docs/debrief.md section 6): "Average score 71 (last 10: 74 ▲)" and
    // "Landings ★★★☆☆ 3.4 (last 10: 3.8)". "" with no measured flight (or no landing).
    Q_PROPERTY(QString averageScoreText READ averageScoreText NOTIFY debriefChanged)
    Q_PROPERTY(QString averageStarsText READ averageStarsText NOTIFY debriefChanged)

public:
    enum Role {
        FlightIdRole = Qt::UserRole + 1,
        DateTextRole,      // "22 Nov 2026" (UTC, block out)
        OutTextRole,       // "10:30Z"
        InTextRole,        // "13:20Z"
        RouteTextRole,     // "KBFI–KSAN", or "KBFI–KSAN, diverted to KSHT"
        FlightNumberRole,  // "XLY8"
        RegistrationRole,
        TypeRole,
        BlockTextRole,     // "2:50"
        FuelUsedLbRole,    // fuel out - fuel in
        TotalTextRole,     // short form: "-$13.1k"
        TotalCentsRole,
        DistanceNmRole,    // flown, from the log's track; -1 when the log no longer has it
        MaxAltFtRole,      // the same
    };

    // Reads nothing yet: call reload().
    // `reviews` gives the review's state while it's being written; without it the state comes from the database.
    FlightListModel(blocktime::db::Database& db, std::filesystem::path logDir, QString airlineIcao,
                    ReviewWriter* reviews = nullptr, QObject* parent = nullptr);
    // Stops a track load that's still running and waits for it (it returns at once when stopped).
    ~FlightListModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(flights_.size()); }
    bool loading() const { return loading_; }
    QString averageScoreText() const { return averageScoreText_; }
    QString averageStarsText() const { return averageStarsText_; }

    // Reads the flights from the database now (it's quick), then the tracks from the log on a worker thread, since 30
    // days of long flights take seconds to read. A reload asked for while tracks are loading runs again when they end.
    Q_INVOKABLE void reload();
    // The same, tracks included, on the calling thread, for tests.
    void reloadBlocking();
    // A review is being written or arrived: the debriefs are read again, the rows (and the selection) stay.
    Q_INVOKABLE void refreshDebrief();
    // The flight's track for the map: the logged one, or its two airports. Empty for a bad row.
    Q_INVOKABLE QGeoPath track(int row) const;
    // What FlightSummary shows: { title, dateText, outText, inText, blockText, fuelText, routeText,
    // income: [{label, amount}], costs: [...], totalText, parkedText, debrief }. Empty for a bad row. `debrief` is
    // { measured, scoreText, starsText, landingPointsText, checks: [{label, detail, state, pointsText}],
    // landingLines, reviewState, reviewError, review: {summary, workOn, landing, atc} } (docs/debrief.md section 6).
    Q_INVOKABLE QVariantMap summary(int row) const;
    Q_INVOKABLE int rowOf(const QString& flightId) const;  // -1 if it isn't listed
    // Opens the replay window for a flight: a second BlockTime process, `--replay <id>`, read-only (D-175).
    Q_INVOKABLE void replay(const QString& flightId) const;

signals:
    void countChanged();
    void loadingChanged();
    void tracksChanged();
    void debriefChanged();

private:
    void readFlights();
    void readDebriefs();
    void applyTracks(std::vector<blocktime::FlightRecord> records);
    void setLoading(bool loading);
    const blocktime::FlightRecord* recordOf(const blocktime::FlightView& f) const;
    QVariantMap debrief(const blocktime::FlightView& f) const;

    blocktime::db::Database& db_;
    std::filesystem::path logDir_;
    QString airlineIcao_;
    std::vector<blocktime::FlightView> flights_;
    std::map<std::string, blocktime::FlightRecord> tracks_;  // by flight id
    ReviewWriter* reviews_;
    std::map<std::string, blocktime::FlightMeasurements> measurements_;  // by flight id
    std::map<std::string, blocktime::StoredReview> written_;             // by flight id
    QString averageScoreText_, averageStarsText_;
    bool loading_ = false;
    bool reloadAgain_ = false;
    std::jthread worker_;  // last, so it's stopped and joined before the members it uses go
};
