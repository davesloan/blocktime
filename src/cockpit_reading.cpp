// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cockpit_reading.h"

#include <cmath>

namespace blocktime {

namespace {

constexpr double kStdHpa = 1013.2;

double number(const nlohmann::json& d, const char* key) {
    return d.contains(key) && d[key].is_number() ? d[key].get<double>() : 0.0;
}
bool flag(const nlohmann::json& d, const char* key) {
    return d.contains(key) && d[key].is_boolean() && d[key].get<bool>();
}

}  // namespace

Family familyOf(std::string_view aircraftFolderOrFamily) {
    return aircraftFolderOrFamily.find("777") != std::string_view::npos ? Family::B777 : Family::B737;
}

bool beltsOn(const nlohmann::json& cockpit) { return number(cockpit, "seat_belts") != 0; }
bool landingLightsOn(const nlohmann::json& cockpit) { return flag(cockpit, "landing_lights"); }
bool gearDown(const nlohmann::json& cockpit) { return flag(cockpit, "gear_down"); }

std::optional<bool> apuSwitchOn(const nlohmann::json& cockpit) {
    if (!cockpit.contains("apu_switch") || !cockpit["apu_switch"].is_boolean()) return std::nullopt;
    return cockpit["apu_switch"].get<bool>();
}

bool altimetersStd(const nlohmann::json& altimeter) {
    return std::abs(number(altimeter, "capt_mb") - kStdHpa) < 0.05 && std::abs(number(altimeter, "fo_mb") - kStdHpa) < 0.05;
}

bool beltsReadable(Family family, bool pmdgSeen) { return family == Family::B777 || pmdgSeen; }

}  // namespace blocktime
