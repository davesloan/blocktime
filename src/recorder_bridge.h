// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What QML sees of the recorder. RecorderBridge lives on the GUI thread; BridgeObserver is called on the recorder
// thread and only ever posts copies to the bridge with queued calls, never touching it directly (D-053).

#pragma once

#include <map>
#include <optional>
#include <string>

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "event_feed_model.h"
#include "flight_stages.h"
#include "recorder.h"

class RecorderBridge : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")

    // "waiting", "starting", "connected", or "blocked" (another recorder holds the mutex)
    Q_PROPERTY(QString connection READ connection NOTIFY connectionChanged)
    Q_PROPERTY(QString logDir READ logDir NOTIFY logDirChanged)

    Q_PROPERTY(bool hasSample READ hasSample NOTIFY sampleChanged)
    Q_PROPERTY(double altitudeFt READ altitudeFt NOTIFY sampleChanged)  // indicated
    Q_PROPERTY(double iasKt READ iasKt NOTIFY sampleChanged)
    Q_PROPERTY(double gsKt READ gsKt NOTIFY sampleChanged)
    Q_PROPERTY(double vsFpm READ vsFpm NOTIFY sampleChanged)
    Q_PROPERTY(double fuelLb READ fuelLb NOTIFY sampleChanged)
    Q_PROPERTY(bool parkingBrake READ parkingBrake NOTIFY sampleChanged)
    Q_PROPERTY(bool onGround READ onGround NOTIFY sampleChanged)
    Q_PROPERTY(int enginesRunning READ enginesRunning NOTIFY sampleChanged)
    Q_PROPERTY(int engineCount READ engineCount NOTIFY sampleChanged)
    // Live's flight panel (D-140): one running flag per engine, and the airspeed tile in Mach at altitude.
    Q_PROPERTY(QVariantList engineRunning READ engineRunning NOTIFY sampleChanged)
    Q_PROPERTY(bool showMach READ showMach NOTIFY sampleChanged)
    Q_PROPERTY(QString machText READ machText NOTIFY sampleChanged)
    Q_PROPERTY(QString aircraft READ aircraft NOTIFY sampleChanged)
    Q_PROPERTY(QString atcId READ atcId NOTIFY sampleChanged)

    Q_PROPERTY(bool flightOpen READ flightOpen NOTIFY flightChanged)
    Q_PROPERTY(QDateTime blockOutTime READ blockOutTime NOTIFY flightChanged)
    Q_PROPERTY(double blockOutFuelLb READ blockOutFuelLb NOTIFY flightChanged)
    Q_PROPERTY(QString lastFlightSummary READ lastFlightSummary NOTIFY flightChanged)

    // The six stages of the open (or last) flight (design.md §13.5, D-128): [{name, state ("upcoming", "current",
    // "done"), lines: [{text, start, end, running, checks: [{text, done}]}]}], and the phase with its sub-state
    // ("Climb — level at 9,000 ft"). Only the Ready line has checks (D-135).
    Q_PROPERTY(QVariantList stages READ stages NOTIFY stagesChanged)
    Q_PROPERTY(QString phase READ phase NOTIFY stagesChanged)
    // The stages show the flight from block out until showNextLeg() (the summary was dismissed), else the next leg,
    // whose Pre-Departure has the GSX services so far (D-134).
    Q_PROPERTY(bool showingFlight READ showingFlight NOTIFY stagesChanged)
    Q_PROPERTY(bool hasNextLegServices READ hasNextLegServices NOTIFY stagesChanged)

    Q_PROPERTY(EventFeedModel* events READ events CONSTANT)

public:
    explicit RecorderBridge(QObject* parent = nullptr);

    QString connection() const { return connection_; }
    QString logDir() const { return logDir_; }
    bool hasSample() const { return hasSample_; }
    double altitudeFt() const { return altitudeFt_; }
    double iasKt() const { return iasKt_; }
    double gsKt() const { return gsKt_; }
    double vsFpm() const { return vsFpm_; }
    double fuelLb() const { return fuelLb_; }
    bool parkingBrake() const { return parkingBrake_; }
    bool onGround() const { return onGround_; }
    int enginesRunning() const { return enginesRunning_; }
    int engineCount() const { return engineCount_; }
    QVariantList engineRunning() const { return engineRunning_; }
    bool showMach() const { return showMach_; }
    QString machText() const { return machText_; }
    QString aircraft() const { return aircraft_; }
    QString atcId() const { return atcId_; }
    bool flightOpen() const { return flightOpen_; }
    QDateTime blockOutTime() const { return blockOutTime_; }
    double blockOutFuelLb() const { return blockOutFuelLb_; }
    QString lastFlightSummary() const { return lastFlightSummary_; }
    EventFeedModel* events() { return &events_; }
    QVariantList stages() const;
    QString phase() const { return QString::fromStdString(stages_.phaseLabel()); }
    bool showingFlight() const { return stages_.showingFlight(); }
    bool hasNextLegServices() const { return stages_.hasNextLegServices(); }
    // The last flight's Arrival is done with: the stages show the next leg.
    Q_INVOKABLE void showNextLeg();

    Q_INVOKABLE void openLogFolder() const;

    // GUI thread only. BridgeObserver posts these.
    void applyStarted(const QString& logDir);
    void applyLine(const QString& line, bool error);
    void applyConnection(blocktime::ConnectionState state);
    void applySample(const blocktime::TelemetrySample& s);
    void applyBlockEvent(const blocktime::BlockEvent& e);
    void applyMatch(const blocktime::MatchResult& m);
    // A flight event the recorder logged (phase_changed, level_off, liftoff, ...); data is its log "data" object.
    void applyFlightEvent(const QString& type, const QVariantMap& data);
    // An airport's runways and parking spots from the recorder (D-147); the latest per ICAO is kept.
    void applyAirport(blocktime::AirportFacilities airport);
    const blocktime::AirportFacilities* airport(const QString& icao) const;
    // The matching check's latest answer (D-088); empty until the first one and after the connection is lost.
    const std::optional<blocktime::MatchResult>& match() const { return match_; }
    // A brake release was refused and the brake hasn't been set since, so no flight can open until it's set and
    // released again (the detector arms only on a new release).
    bool releaseRefused() const { return releaseRefused_; }
    // Another recorder holds the single-instance mutex, so this app doesn't record.
    void setBlocked();
    // Recorder::run() returned; non-zero means the log couldn't be written.
    void applyStopped(int result);

signals:
    void connectionChanged();
    void logDirChanged();
    void sampleChanged();
    void flightChanged();
    void matchChanged();
    void releaseRefusedChanged();
    void stagesChanged();
    // Every block event, after it's applied: "block_out", "block_in", "flight_interrupted" or "block_out_refused",
    // with its flight id ("" for a refusal).
    void blockEvent(const QString& type, const QString& flightId);
    // Every flight event, after it's applied.
    void flightEvent(const QString& type, const QVariantMap& data);
    void airportLoaded(const QString& icao);

private:
    std::map<std::string, blocktime::AirportFacilities> airports_;
    void setConnection(const QString& c);

    QString connection_ = "waiting";
    QString logDir_;
    bool hasSample_ = false;
    double altitudeFt_ = 0, iasKt_ = 0, gsKt_ = 0, vsFpm_ = 0, fuelLb_ = 0;
    bool parkingBrake_ = false, onGround_ = false;
    int enginesRunning_ = 0, engineCount_ = 0;
    QString aircraft_, atcId_;
    QVariantList engineRunning_;
    bool showMach_ = false;
    QString machText_;
    bool flightOpen_ = false;
    QDateTime blockOutTime_;
    double blockOutFuelLb_ = 0;
    QString lastFlightSummary_;
    std::optional<blocktime::MatchResult> match_;
    bool releaseRefused_ = false;
    void setReleaseRefused(bool refused);
    EventFeedModel events_;
    blocktime::FlightStages stages_;
};

// Runs on the recorder thread. Copies what it gets and posts it to the bridge on the GUI thread.
class BridgeObserver : public blocktime::RecorderObserver {
public:
    explicit BridgeObserver(RecorderBridge& bridge) : bridge_(bridge) {}
    void onStarted(const std::filesystem::path& logDir) override;
    void onLine(const std::string& line, bool error) override;
    void onConnection(blocktime::ConnectionState state) override;
    void onTelemetry(const blocktime::TelemetrySample& s) override;
    void onBlockEvent(const blocktime::BlockEvent& e) override;
    void onMatch(const blocktime::MatchResult& m) override;
    void onFlightEvent(const std::string& type, const nlohmann::json& data) override;
    void onAirport(const blocktime::AirportFacilities& airport) override;

private:
    RecorderBridge& bridge_;  // only used as the target of queued calls
};
