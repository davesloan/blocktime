// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "feed_text.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <type_traits>
#include <variant>

#include "event_log.h"

namespace blocktime {

namespace {

std::string hhmmss(TimePoint t) { return formatUtc(t).substr(11, 8); }

std::string str(const nlohmann::json& d, const char* key) {
    return d.is_object() ? d.value(key, std::string()) : std::string();
}

double num(const nlohmann::json& d, const char* key) {
    if (!d.is_object()) return 0;
    const auto it = d.find(key);
    return it != d.end() && it->is_number() ? it->get<double>() : 0.0;
}

}  // namespace

std::string feedText(std::string_view type, const nlohmann::json& d) {
    if (type == "phase_changed") {
        std::string phase = str(d, "phase");
        std::transform(phase.begin(), phase.end(), phase.begin(), [](unsigned char c) {
            return c == '_' ? ' ' : static_cast<char>(std::toupper(c));
        });
        return phase;
    }
    if (type == "liftoff") return "LIFTOFF  " + str(d, "runway");
    if (type == "lined_up") return std::format("LINED UP  runway {}  {:.1f} nm", str(d, "runway"), num(d, "distance_nm"));
    if (type == "gsx_service") {
        const std::string state = str(d, "state");
        if (state != "requested" && state != "performing" && state != "completed") return "";
        return "GSX " + str(d, "service") + " " + state;
    }
    if (type == "touchdown") return std::format("TOUCHDOWN  {:.0f} fpm  {:.2f} g", num(d, "vs_fpm"), num(d, "g"));
    if (type == "arrival_gate") return "ARRIVAL GATE  " + str(d, "gate") + "  (" + str(d, "result") + ")";
    return "";
}

std::string blockFeedText(const BlockEvent& event) {
    return std::visit(
        [](const auto& e) -> std::string {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, BlockOut>) {
                return std::format("BLOCK OUT  {}  released {}  fuel {:.0f} lb", e.atcId, hhmmss(e.time), e.fuelLb);
            } else if constexpr (std::is_same_v<T, BlockIn>) {
                return std::format("BLOCK IN  brake set {}  fuel {:.0f} lb  block {}:{:02}", hhmmss(e.time), e.fuelLb,
                                   e.blockTimeS / 3600, e.blockTimeS / 60 % 60);
            } else if constexpr (std::is_same_v<T, BlockOutRefused>) {
                return std::format("NOT RECORDING  {}", problemCode(e.match.problem));
            } else {
                return std::format("FLIGHT INTERRUPTED  ({})", e.reason);
            }
        },
        event);
}

}  // namespace blocktime
