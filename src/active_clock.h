// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Time that counts toward phase-detection holds (D-058). Each message from the sim adds the time since the previous
// one, capped at kMaxGap, so a sim freeze (no messages at all) adds almost nothing, however long it lasts.

#pragma once

#include <algorithm>
#include <chrono>
#include <optional>

namespace blocktime {

class ActiveClock {
public:
    static constexpr std::chrono::milliseconds kMaxGap{2000};

    // `counts` is false while the sim is paused: it still answers the recorder's heartbeat, but that time mustn't
    // count either, or a long pause would look like a held level.
    void onMessage(std::chrono::steady_clock::time_point t, bool counts = true) {
        if (counts && last_ && t > *last_) {
            const auto gap = std::min<std::chrono::steady_clock::duration>(t - *last_, kMaxGap);
            seconds_ += std::chrono::duration<double>(gap).count();
        }
        if (!last_ || t > *last_) last_ = t;
    }

    double seconds() const { return seconds_; }

private:
    std::optional<std::chrono::steady_clock::time_point> last_;
    double seconds_ = 0;
};

}  // namespace blocktime
