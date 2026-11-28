// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// New Flight's state (design.md §13.1): the airplane, the kind of flight, its loads, the destination and the flight
// number. Each kind's loads are rolled once, the first time it's chosen, and the belly cargo option only adds or
// removes the rolled cargo, so no toggle ever re-rolls (D-089).

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "loads.h"
#include "planned_flight.h"
#include "pricing.h"
#include "sayintentions.h"

namespace blocktime {

class FlightDraft {
public:
    // `daytime` picks the passenger band (D-104). Chooses the first allowed kind and rolls its loads.
    FlightDraft(FlightAirplane airplane, Rates rates, bool daytime, Roll roll);  // locks rates.fuelPer100Lb (D-107)
    // A draft of a flight already planned (Change flight, review 4.5): its kind, loads, destination, flight number and
    // extra fuel, and its locked fuel price. Rolls nothing (D-089); `roll` is only for a kind chosen later.
    static FlightDraft fromPlanned(FlightAirplane airplane, Rates rates, const PlannedFlightView& flight, bool daytime,
                                   Roll roll);

    const FlightAirplane& airplane() const { return airplane_; }
    const std::vector<FlightKind>& kinds() const { return kinds_; }
    FlightKind kind() const { return kind_; }
    void setKind(FlightKind kind);  // rolls that kind the first time it's chosen, keeps it after

    bool bellyCargoAvailable() const;  // airliners only (D-089)
    bool bellyCargo() const { return bellyCargo_; }
    void setBellyCargo(bool on) { bellyCargo_ = on; }

    bool fullyLoadedAvailable() const;  // passenger and cargo flights (D-215, D-251)
    bool fullyLoaded() const { return fullyLoaded_ && fullyLoadedAvailable(); }
    void setFullyLoaded(bool on) { fullyLoaded_ = on; }  // the random roll is kept for switching back

    int additionalFuelLb() const { return additionalFuelLb_; }
    void setAdditionalFuelLb(int lb) { additionalFuelLb_ = lb; }

    const DestinationCheck& destination() const { return destination_; }
    void setDestination(DestinationCheck check) { destination_ = std::move(check); }

    const std::string& flightNumber() const { return flightNumber_; }  // starts as the airplane's default (D-106)
    void setFlightNumber(std::string number) { flightNumber_ = std::move(number); }

    LoadRoll loads() const;                  // cargoLb is 0 when belly cargo is off
    std::vector<HoldLoad> holds() const;     // bags and cargo split across the holds
    std::optional<Preview> preview() const;  // nullopt until the destination is ok
    bool canPlan() const;                    // the destination is ok and the flight number valid

    NewPlannedFlight toNew(std::string plannedAt, const AtisRunways& runways) const;

private:
    FlightDraft(FlightAirplane airplane, Rates rates, bool daytime, Roll roll, const PlannedFlightView& flight);

    FlightAirplane airplane_;
    Rates rates_;
    bool daytime_;
    Roll roll_;
    std::vector<FlightKind> kinds_;
    FlightKind kind_;
    std::map<FlightKind, LoadRoll> rolled_;
    bool bellyCargo_ = true;  // starts on (D-089)
    bool fullyLoaded_ = false;
    int additionalFuelLb_ = 0;
    DestinationCheck destination_;
    std::string flightNumber_;
};

}  // namespace blocktime
