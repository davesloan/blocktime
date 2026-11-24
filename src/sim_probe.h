// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The probe (D-116), on only with --probe: logs what milestones 7-10 need to know but nobody has seen yet.
// - probe_gsx: GSX Pro's L:vars (names from community tools, unverified), on change.
// - probe_simvars: the light, sign, APU and gear SimVars, on change, to compare with PMDG's own values.
// - probe_pmdg_737 / probe_pmdg_777: the PMDG SDK switch values (pmdg_probe.h), on change, at most once a second.
// - probe_facility: an airport's runways and parking spots from the sim's facility data, with each record's raw bytes
//   so a field read with the wrong type can still be decoded afterwards.
// See docs/notes/probe-flight.md.

#pragma once

#include <chrono>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "event_log.h"

namespace blocktime {

class SimProbe {
public:
    explicit SimProbe(EventLog& log) : log_(log) {}

    // Registers the probe's data definitions on a new SimConnect session (`handle` is the HANDLE). False if one of the
    // SimConnect calls failed; the rest still go ahead.
    bool setup(void* handle);
    // True for request ids the probe owns; SimConnection hands their messages here.
    static bool owns(unsigned long requestId);

    void onSimObjectData(unsigned long requestId, const void* data, std::size_t size);
    void onClientData(unsigned long requestId, const void* data, std::size_t size);
    void onFacilityData(const void* recv);  // a SIMCONNECT_RECV_FACILITY_DATA
    void onFacilityDataEnd(unsigned long requestId);
    // Asks for each airport's facility data; each is logged when its data ends.
    void requestFacilities(void* handle, const std::vector<std::string>& icaos);
    // Writes PMDG values held back by the once-a-second limit. Called from the recorder's loop.
    void tick();

private:
    struct Pending {
        std::string type;
        nlohmann::json last, waiting;
        std::chrono::steady_clock::time_point lastWritten{};
    };
    void offer(Pending& p, nlohmann::json values);

    EventLog& log_;
    nlohmann::json lastGsx_, lastSimVars_, lastBaro_;
    Pending pmdg737_{"probe_pmdg_737", nullptr, nullptr, {}};
    Pending pmdg777_{"probe_pmdg_777", nullptr, nullptr, {}};
    std::map<unsigned long, nlohmann::json> facilities_;  // by request id, until the data ends
};

}  // namespace blocktime
