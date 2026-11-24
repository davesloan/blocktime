// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Whether to ask SayIntentions for BlockTime's arrival gate, once SayIntentions has picked its own, and the
// arrival_gate event that records what happened (D-141, D-147). One try per flight; FlightWatcher runs it.

#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "gate_picker.h"
#include "parking.h"
#include "sayintentions.h"

namespace blocktime {

struct GatePlan {
    bool send = false;
    std::string shortName;  // to send to assignGate
    // arrival_gate: icao, runway, si_gate, gate, reason, sent, result, si_parking, error. Complete when !send;
    // finishSend completes it after a send. FlightWatcher adds flight_id and time.
    nlohmann::json event;
};

// nullopt while SayIntentions has no gate yet, or its destination isn't `destinationIcao`.
std::optional<GatePlan> planGate(const SiFlight& si, const std::string& destinationIcao,
                                 const AirportFacilities& airport, const GateContext& context);

// After assignGate: result "assigned", or "unknown_gate" (SayIntentions didn't know the name) or "error".
nlohmann::json finishSend(nlohmann::json event, const std::string& error, const std::optional<SiParking>& readBack);

}  // namespace blocktime
