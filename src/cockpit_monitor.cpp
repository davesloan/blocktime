// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cockpit_monitor.h"

namespace blocktime {

void CockpitMonitor::onPlain(const PlainCockpit& p, TimePoint) {
    if (p.paused) return;
    plain_ = p;
}

void CockpitMonitor::onPmdg(const PmdgCockpit& p, TimePoint) { pmdg_ = p; }

nlohmann::json CockpitMonitor::current() const {
    nlohmann::json e = {
        {"landing_lights", pmdg_ ? pmdg_->landingLights : plain_->landingLight},
        {"seat_belts", pmdg_ ? pmdg_->seatBelts : (plain_->seatBeltSign ? 2 : 0)},
        {"gear_down", plain_->gearPct >= kGearDownPct},
        {"pmdg", pmdg_.has_value()},
    };
    if (pmdg_) {
        e["vref_kt"] = pmdg_->vrefKt;
        e["landing_flaps"] = pmdg_->landingFlaps;
        e["autopilot"] = pmdg_->autopilot;
    }
    return e;
}

std::optional<nlohmann::json> CockpitMonitor::due(TimePoint now) {
    if (!plain_) return std::nullopt;  // the gear comes only from the plain sample
    if (lastWritten_ && now - *lastWritten_ < std::chrono::seconds(1)) return std::nullopt;
    nlohmann::json e = current();
    if (e == last_) return std::nullopt;
    last_ = e;
    lastWritten_ = now;
    return e;
}

void CockpitMonitor::reset() {
    plain_.reset();
    pmdg_.reset();
    last_ = nullptr;
    lastWritten_.reset();
}

}  // namespace blocktime
