// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "phase_pill.h"

#include <algorithm>
#include <array>

namespace blocktime {
namespace {

constexpr std::array<std::string_view, 3> kDeparture{"pushback", "taxi_out", "takeoff"};
constexpr std::array<std::string_view, 4> kDescent{"descent", "approach", "landing", "taxi_in"};

template <std::size_t N>
std::vector<StageState> divisions(const std::array<std::string_view, N>& phases, StageState stage,
                                  std::string_view phase) {
    if (stage != StageState::Current) return std::vector<StageState>(N, stage);
    if (phase == "go_around") phase = "approach";
    const auto at = std::find(phases.begin(), phases.end(), phase);
    std::vector<StageState> out;
    for (auto it = phases.begin(); it != phases.end(); ++it) {
        out.push_back(at == phases.end() ? StageState::Upcoming
                      : it < at          ? StageState::Done
                      : it == at         ? StageState::Current
                                         : StageState::Upcoming);
    }
    return out;
}

}  // namespace

std::vector<PillSection> pillSections(const std::vector<StageView>& stages, std::string_view phase) {
    std::vector<PillSection> out;
    for (const StageView& s : stages) {
        PillSection p{s.name, s.state, {}};
        if (s.stage == Stage::Departure) p.divisions = divisions(kDeparture, s.state, phase);
        if (s.stage == Stage::Descent) p.divisions = divisions(kDescent, s.state, phase);
        out.push_back(std::move(p));
    }
    return out;
}

}  // namespace blocktime
