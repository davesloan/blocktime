// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gate_assigner.h"

#include "geo.h"

namespace blocktime {

std::optional<GatePlan> planGate(const SiFlight& si, const std::string& destinationIcao,
                                 const AirportFacilities& airport, const GateContext& context) {
    if (si.assignedGate.empty() || si.destination != destinationIcao) return std::nullopt;
    // Before departure assigned_gate may be the departure gate: SayIntentions has picked its arrival gate only once
    // that gate is at the destination (D-141).
    if (!si.gateLat || !si.gateLon ||
        distanceMeters(*si.gateLat, *si.gateLon, airport.lat, airport.lon) > kSiGateMaxFromAirportM) {
        return std::nullopt;
    }
    // SayIntentions' gate is matched to an MSFS spot by its position: its names aren't always MSFS's (D-147).
    const auto siSpot = nearestSpot(airport.spots, *si.gateLat, *si.gateLon, kParkedSpotMaxM);
    const auto pick = pickGate(airport, siSpot, context);
    GatePlan plan;
    plan.siLat = *si.gateLat;
    plan.siLon = *si.gateLon;
    plan.event = {{"icao", destinationIcao},
                  {"runway", si.arrivingRunway},
                  {"si_gate", si.assignedGate},
                  {"gate", ""},
                  {"reason", ""},
                  {"sent", false},
                  {"result", "kept"},
                  {"si_parking", ""},
                  {"error", ""}};
    if (!pick) {
        if (siSpot && !spotFits(*siSpot, context.wingspanM)) {
            plan.event["result"] = "none_fits";
        } else if (siSpot) {
            plan.event["gate"] = spotName(*siSpot);
        }
        return plan;
    }
    plan.event["gate"] = spotName(pick->spot);
    plan.event["reason"] = std::string(pickReasonCode(pick->reason));
    plan.send = true;
    plan.shortName = spotShortName(pick->spot);
    return plan;
}

nlohmann::json finishSend(const GatePlan& plan, const std::string& error, const std::optional<SiParking>& readBack) {
    nlohmann::json event = plan.event;
    event["sent"] = true;
    event["error"] = error;
    if (!error.empty()) {
        event["result"] = error.find("Unknown gate") != std::string::npos ? "unknown_gate" : "error";
    } else if (!readBack) {
        event["result"] = "unconfirmed";
    } else {
        event["si_parking"] = readBack->name;
        const bool stayed = distanceMeters(readBack->lat, readBack->lon, plan.siLat, plan.siLon) <= kParkedSpotMaxM;
        event["result"] = stayed ? "ignored" : "assigned";
    }
    return event;
}

}  // namespace blocktime
