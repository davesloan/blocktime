// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gate_assigner.h"

namespace blocktime {

namespace {

bool sameSpot(const ParkingSpot& a, const ParkingSpot& b) {
    return a.name == b.name && a.number == b.number && a.type == b.type && a.lat == b.lat && a.lon == b.lon;
}

}  // namespace

std::optional<GatePlan> planGate(const SiFlight& si, const std::string& destinationIcao,
                                 const AirportFacilities& airport, const GateContext& context) {
    if (si.assignedGate.empty() || si.destination != destinationIcao) return std::nullopt;
    const auto pick = pickGate(airport, si.arrivingRunway, context);
    GatePlan plan;
    plan.event = {{"icao", destinationIcao},
                  {"runway", si.arrivingRunway},
                  {"si_gate", si.assignedGate},
                  {"gate", pick ? spotName(pick->spot) : ""},
                  {"reason", pick ? std::string(pickReasonCode(pick->reason)) : "none_fits"},
                  {"sent", false},
                  {"result", ""},
                  {"si_parking", ""},
                  {"error", ""}};
    if (!pick) {
        plan.event["result"] = "none_fits";
        return plan;
    }
    // SayIntentions' gate is matched to an MSFS spot by its position: its names aren't always MSFS's (D-147).
    std::optional<ParkingSpot> siSpot;
    if (si.gateLat && si.gateLon) siSpot = nearestSpot(airport.spots, *si.gateLat, *si.gateLon);
    if (siSpot && sameSpot(*siSpot, pick->spot)) {
        plan.event["result"] = "same_as_si";
        return plan;
    }
    plan.send = true;
    plan.shortName = spotShortName(pick->spot);
    return plan;
}

nlohmann::json finishSend(nlohmann::json event, const std::string& error, const std::optional<SiParking>& readBack) {
    event["sent"] = true;
    event["error"] = error;
    if (error.empty()) {
        event["result"] = "assigned";
        event["si_parking"] = readBack ? readBack->name : "";
    } else {
        event["result"] = error.find("Unknown gate") != std::string::npos ? "unknown_gate" : "error";
    }
    return event;
}

}  // namespace blocktime
