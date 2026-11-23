// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_draft.h"

#include <utility>

namespace blocktime {

FlightDraft::FlightDraft(FlightAirplane airplane, Cents fuelPriceCentsPer100Lb, bool daytime, Roll roll)
    : airplane_(std::move(airplane)),
      fuelPrice_(fuelPriceCentsPer100Lb),
      daytime_(daytime),
      roll_(std::move(roll)),
      kinds_(allowedKinds(airplane_)),
      kind_(kinds_.front()),
      flightNumber_(airplane_.freeFlightNumber) {
    setKind(kind_);
}

void FlightDraft::setKind(FlightKind kind) {
    kind_ = kind;
    if (!rolled_.contains(kind)) {
        rolled_[kind] = rollLoads(airplane_.cabins, airplane_.holds, airplane_.limits, kind, daytime_, roll_);
    }
}

bool FlightDraft::bellyCargoAvailable() const { return kind_ == FlightKind::Passenger; }

LoadRoll FlightDraft::loads() const {
    LoadRoll l = rolled_.at(kind_);
    if (bellyCargoAvailable() && !bellyCargo_) l.cargoLb = 0;
    return l;
}

std::vector<HoldLoad> FlightDraft::holds() const {
    const LoadRoll l = loads();
    return splitHolds(airplane_.holds, bagsLb(l.cabins, airplane_.limits) + l.cargoLb);
}

std::optional<Preview> FlightDraft::preview() const {
    if (!destination_.ok) return std::nullopt;
    const LoadRoll l = loads();
    PriceInput in;
    in.kind = kind_;
    in.family = airplane_.family;
    in.role = airplane_.role;
    for (const Cabin& c : airplane_.cabins) in.seatsInstalled += c.seats;
    in.cabins = l.cabins;
    in.cargoLb = l.cargoLb;
    in.distanceNm = destination_.distanceNm;
    in.blockHours = estimatedBlockHours(destination_.distanceNm, airplane_.cruiseSpeedKt);
    in.mlwLb = airplane_.limits.mlwLb;
    in.fuelPriceCentsPer100Lb = fuelPrice_;
    return pricePreview(in);
}

bool FlightDraft::canPlan() const { return destination_.ok && validFlightNumber(flightNumber_); }

NewPlannedFlight FlightDraft::toNew(std::string plannedAt, const AtisRunways& runways) const {
    const LoadRoll l = loads();
    NewPlannedFlight n;
    n.airplaneId = airplane_.airplaneId;
    n.originIcao = airplane_.originIcao;
    n.destinationIcao = destination_.icao;
    n.flightNumber = flightNumber_;
    n.kind = kind_;
    n.cabins = l.cabins;
    n.cargoLb = l.cargoLb;
    n.fuelPriceCentsPer100Lb = fuelPrice_;
    n.plannedAt = std::move(plannedAt);
    n.originRunway = runways.origin;
    n.destinationRunway = runways.destination;
    return n;
}

}  // namespace blocktime
