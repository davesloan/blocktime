// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GSX's services as spans, each given its leg (D-138), from the log's gsx_service, block and sim session events in
// log order. Live (FlightStages) and settling (flight_settlement.h) both read services through this, so they agree.
// - A service starts when it's performing, and is complete when it then goes to completed or back to available
//   (boarding and pushback skip completed). Bypassed or not_available while performing abandons it: it's dropped.
// - Catering, refuelling and boarding belong to the next block out (nextLeg), counted from the later of the last
//   block in and the last sim_stopped / sim_disconnected (not BlockTime's own exit, recorder_exit: the sim carries
//   on). Deboarding belongs to the last block in, until the next block out (arrival). An interrupted flight drops
//   its services (D-072).
// - Pushback is never a span: the phases time it. Only whether GSX was pushing at block out is kept.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace blocktime {

struct ServiceSpan {
    std::string service;           // catering, refuelling, boarding, deboarding
    std::string startIso, endIso;  // endIso "" while running
    bool running = true;
};

class GsxTimeline {
public:
    // gsx_service, block_out, block_in, flight_interrupted, sim_stopped, sim_disconnected; others are ignored. The
    // time is data "time" (ISO 8601 UTC); the caller puts it there for the sim_* lines, which carry none.
    void onEvent(std::string_view type, const nlohmann::json& data);

    const std::vector<ServiceSpan>& nextLeg() const { return nextLeg_; }    // not yet taken by a block out
    const std::vector<ServiceSpan>& departed() const { return departed_; }  // completed ones the last block out took
    const std::vector<ServiceSpan>& arrival() const { return arrival_; }    // deboarding since the last block in
    bool pushbackByGsx() const { return pushbackByGsx_; }  // pushback requested or performing at the last block out
    const std::string& lastBlockInFlightId() const { return lastBlockIn_; }  // "" before a block in, after a block out

private:
    void onService(const std::string& service, const std::string& state, const std::string& iso);

    std::vector<ServiceSpan> nextLeg_, departed_, arrival_;
    std::string pushbackState_;
    bool pushbackByGsx_ = false;
    std::string lastBlockIn_;
};

}  // namespace blocktime
