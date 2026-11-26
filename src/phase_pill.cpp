// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "phase_pill.h"

#include <algorithm>
#include <array>
#include <vector>

namespace blocktime {
namespace {

struct Part {
    std::string_view label;
    std::vector<std::string_view> phases;  // the recorder's phases that put the marker here
};

// Each stage's parts in order. A phase none of them names leaves the marker on the first part.
const std::array<std::vector<Part>, 6> kParts{{
    {{"Preparing", {}}, {"Boarding", {}}},
    {{"Pushback", {"pushback"}}, {"Taxiing", {"taxi_out"}}, {"Takeoff", {"takeoff"}}},
    {{"", {"climb"}}},
    {{"", {"cruise"}}},
    {{"Descending", {"descent"}}, {"Approach", {"approach", "go_around"}}},
    {{"Landing", {"landing"}}, {"Taxiing", {"taxi_in"}}, {"At Gate", {"arrived"}}},
}};

}  // namespace

std::vector<PillSection> pillSections(const std::vector<StageView>& stages) {
    std::vector<PillSection> out;
    for (const StageView& s : stages) out.push_back({s.name, s.state});
    return out;
}

PillMarker pillMarker(const std::vector<StageView>& stages, std::string_view phase, bool boardingStarted) {
    PillMarker m;
    const auto current = std::find_if(stages.begin(), stages.end(),
                                      [](const StageView& s) { return s.state == StageState::Current; });
    if (current == stages.end()) return m;
    m.section = static_cast<int>(current->stage);
    const std::vector<Part>& parts = kParts[static_cast<std::size_t>(m.section)];
    m.parts = static_cast<int>(parts.size());
    if (current->stage == Stage::PreDeparture) {
        m.part = boardingStarted ? 1 : 0;
    } else {
        for (std::size_t i = 0; i < parts.size(); ++i) {
            const auto& ph = parts[i].phases;
            if (std::find(ph.begin(), ph.end(), phase) != ph.end()) m.part = static_cast<int>(i);
        }
    }
    m.label = parts[static_cast<std::size_t>(m.part)].label;
    return m;
}

}  // namespace blocktime
