// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Pre-Flight panel's memory (D-153, D-188): what the origin's ATC and ATIS said since the flight was planned, fed
// every flight and block event in log order, like FlightStages. Only the origin's messages count, and only from the
// planning time on, so a turnaround or a restart that re-logs SayIntentions' history never shows the last leg's
// clearance. Block in starts over. Then the departure and clearance views, with SimBrief's runway and SID until ATC
// changes them (D-189).

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "clearance.h"
#include "planned_flight.h"

namespace blocktime {

class PreFlightLog {
public:
    // A different origin or planning time starts over.
    void setFlight(std::string originIcao, std::string plannedAt /* UTC instant */);
    // atc_exchange, atis, block_out, liftoff, block_in and flight_interrupted; anything else is ignored.
    void onEvent(std::string_view type, const nlohmann::json& data);

    const std::optional<Clearance>& clearance() const { return clearance_; }  // the latest by ATC's time
    std::optional<TimePoint> clearedAt() const { return clearedAt_; }
    const std::optional<TaxiInstruction>& taxi() const { return taxi_; }      // the latest before liftoff
    std::string atcRunway() const;  // the taxi instruction's runway, else the clearance's; ""
    const nlohmann::json* atis() const { return atis_ ? &*atis_ : nullptr; }  // the origin's latest `atis` data
    bool blockedOut() const { return blockedOut_; }
    bool airborne() const { return airborne_; }

private:
    void reset();

    std::string origin_, plannedAt_;
    std::optional<TimePoint> plannedTime_;
    std::optional<Clearance> clearance_;
    std::optional<TimePoint> clearedAt_;
    std::optional<TaxiInstruction> taxi_;
    std::optional<TimePoint> taxiAt_;
    std::optional<nlohmann::json> atis_;
    bool blockedOut_ = false, airborne_ = false;
};

struct DepartureView {
    std::string runway, planRunway;  // runway: ATC's, else the ATIS's first departing, else the plan's
    bool runwayChanged = false;      // both set and different
    std::optional<int> headingDeg;   // the takeoff report's magnetic course for `runway`
    std::string condition;           // "Wet" with precipitation in the METAR, else the plan's surface ("Dry"); ""
    std::string wind, temperature, altimeter, atisLetter;  // "" before an ATIS; temperature "19 °C"
};
DepartureView departureView(const PlannedFlightView& flight, const PreFlightLog& log);

struct ClearanceView {
    bool received = false;
    // The plan's full form "PADRZ2 IKAYE" from the start (D-189). Once cleared: the plan's form when ATC's SID has the
    // plan's name (or ATC named none), else ATC's own, marked changed.
    std::string sid;
    bool sidChanged = false;
    std::string initialAltitude, squawk, departureFrequency, taxiRoute;  // "" until ATC gives them
};
ClearanceView clearanceView(const PlannedFlightView& flight, const PreFlightLog& log);

}  // namespace blocktime
