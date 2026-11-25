// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Whether to ask SayIntentions for BlockTime's arrival gate, once SayIntentions has picked its own, and the
// arrival_gate event that records what happened (D-141, D-147, D-155). One try per flight; FlightWatcher runs it.

#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "gate_picker.h"
#include "parking.h"
#include "sayintentions.h"

namespace blocktime {

// SayIntentions' gate counts as an arrival gate only within this of the destination's reference point.
inline constexpr double kSiGateMaxFromAirportM = 5000.0;

struct GatePlan {
    bool send = false;
    std::string shortName;      // to send to assignGate
    double siLat = 0, siLon = 0;  // SayIntentions' own gate, to tell from the read-back whether it moved
    // arrival_gate: icao, runway, si_gate, gate, reason, sent, result, si_parking, error. Complete when !send;
    // finishSend completes it after a send. FlightWatcher adds flight_id and time.
    nlohmann::json event;
};

// nullopt while SayIntentions has no arrival gate yet (none, or one without a position at the destination: before
// departure it may be the departure gate), or its destination isn't `destinationIcao`.
std::optional<GatePlan> planGate(const SiFlight& si, const std::string& destinationIcao,
                                 const AirportFacilities& airport, const GateContext& context);

// Results without a send: "kept" (SayIntentions' gate stands) or "none_fits" (it's too small and nothing larger is
// there). After assignGate: "assigned" (getParking reads back a spot away from SayIntentions' own gate), "ignored"
// (it reads back SayIntentions' own: assignGate can answer without an error and change nothing), "unconfirmed" (no
// read-back), "unknown_gate" (SayIntentions didn't know the name) or "error".
nlohmann::json finishSend(const GatePlan& plan, const std::string& error, const std::optional<SiParking>& readBack);

}  // namespace blocktime
