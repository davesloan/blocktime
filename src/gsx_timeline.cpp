// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gsx_timeline.h"

#include <algorithm>

namespace blocktime {

namespace {

std::string str(const nlohmann::json& d, const char* key) {
    const auto f = d.find(key);
    return f != d.end() && f->is_string() ? f->get<std::string>() : std::string();
}

void dropRunning(std::vector<ServiceSpan>& spans) {
    std::erase_if(spans, [](const ServiceSpan& s) { return s.running; });
}

}  // namespace

void GsxTimeline::onService(const std::string& service, const std::string& state, const std::string& iso) {
    if (service == "pushback") {
        pushbackState_ = state;
        return;
    }
    const bool deboarding = service == "deboarding";
    if (deboarding && lastBlockIn_.empty()) return;  // only after a block in, until the next block out
    std::vector<ServiceSpan>& spans = deboarding ? arrival_ : nextLeg_;
    const auto running = std::find_if(spans.begin(), spans.end(),
                                      [&](const ServiceSpan& s) { return s.running && s.service == service; });
    if (state == "performing") {
        if (running == spans.end()) spans.push_back(ServiceSpan{service, iso, "", true});
    } else if (running != spans.end()) {
        if (state == "completed" || state == "available") {
            running->running = false;
            running->endIso = iso;
        } else if (state == "bypassed" || state == "not_available") {
            spans.erase(running);  // abandoned
        }
    }
}

void GsxTimeline::onEvent(std::string_view type, const nlohmann::json& d) {
    if (type == "gsx_service") {
        onService(str(d, "service"), str(d, "state"), str(d, "time"));
    } else if (type == "block_out") {
        dropRunning(nextLeg_);
        departed_ = std::move(nextLeg_);
        nextLeg_.clear();
        arrival_.clear();
        lastBlockIn_.clear();
        pushbackByGsx_ = pushbackState_ == "requested" || pushbackState_ == "performing";
    } else if (type == "block_in") {
        nextLeg_.clear();
        arrival_.clear();
        lastBlockIn_ = str(d, "flight_id");
    } else if (type == "flight_interrupted") {
        nextLeg_.clear();
        departed_.clear();
        arrival_.clear();
        lastBlockIn_.clear();
    } else if (type == "sim_stopped" || (type == "sim_disconnected" && str(d, "reason") != "recorder_exit")) {
        // BlockTime closing (recorder_exit) leaves the sim's session, and GSX's services, as they were.
        nextLeg_.clear();
        dropRunning(arrival_);
        pushbackState_.clear();
    }
}

}  // namespace blocktime
