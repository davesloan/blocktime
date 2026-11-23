// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "loads.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <numeric>
#include <utility>

namespace blocktime {

namespace {

constexpr std::array<std::pair<FlightKind, std::string_view>, 5> kCodes{{
    {FlightKind::Passenger, "passenger"},
    {FlightKind::Vip, "vip"},
    {FlightKind::Charter, "charter"},
    {FlightKind::Cargo, "cargo"},
    {FlightKind::Delivery, "delivery"},
}};

struct Band {
    double lo, hi;
};

// design.md §9.
Band passengerBand(FlightKind kind, bool daytime) {
    if (kind == FlightKind::Charter) return {0.10, 1.00};
    return daytime ? Band{0.90, 1.00} : Band{0.40, 0.80};
}

int fraction(Band band, double r, int of) { return static_cast<int>(std::floor(of * (band.lo + (band.hi - band.lo) * r))); }

int holdCapacity(const std::vector<Hold>& holds) {
    return std::accumulate(holds.begin(), holds.end(), 0, [](int sum, const Hold& h) { return sum + h.maxLb; });
}

}  // namespace

std::string_view kindCode(FlightKind kind) {
    for (const auto& [k, code] : kCodes) {
        if (k == kind) return code;
    }
    return "passenger";
}

std::optional<FlightKind> parseKind(std::string_view code) {
    for (const auto& [k, c] : kCodes) {
        if (c == code) return k;
    }
    return std::nullopt;
}

LoadRoll rollLoads(const std::vector<Cabin>& cabins, const std::vector<Hold>& holds, const AirplaneLimits& limits,
                   FlightKind kind, bool daytime, const Roll& roll) {
    LoadRoll out;
    if (kind == FlightKind::Delivery) return out;
    if (kind == FlightKind::Cargo) {
        out.cargoLb = std::min(fraction({0.50, 1.00}, roll(), holdCapacity(holds)), limits.maxPayloadLb);
        return out;
    }
    const Band band = passengerBand(kind, daytime);
    int people = 0;
    for (const Cabin& c : cabins) {
        out.cabins.push_back({c.cabinClass, fraction(band, roll(), c.seats)});
        people += out.cabins.back().passengers;
    }
    if (kind == FlightKind::Passenger) {
        const int bags = bagsLb(out.cabins, limits);
        const int space = std::max(0, holdCapacity(holds) - bags);
        const int payloadLeft = std::max(0, limits.maxPayloadLb - people * limits.passengerWeightLb - bags);
        out.cargoLb = std::min(fraction({0.10, 1.00}, roll(), space), payloadLeft);
    }
    return out;
}

int bagsLb(const std::vector<CabinLoad>& cabins, const AirplaneLimits& limits) {
    int people = 0;
    for (const CabinLoad& c : cabins) people += c.passengers;
    return people * limits.bagWeightLb;
}

std::vector<HoldLoad> splitHolds(const std::vector<Hold>& holds, int totalLb) {
    assert(totalLb <= holdCapacity(holds));
    std::vector<HoldLoad> out;
    for (const Hold& h : holds) out.push_back({h.hold, 0});
    std::vector<bool> open(holds.size(), true);
    int remaining = totalLb;
    // Share by ratio among open holds; a hold that reaches its max closes, and the rest is shared again.
    while (remaining > 0) {
        int ratioSum = 0;
        for (std::size_t i = 0; i < holds.size(); ++i) {
            if (open[i]) ratioSum += holds[i].ratio;
        }
        if (ratioSum == 0) break;
        bool closedOne = false;
        int given = 0;
        for (std::size_t i = 0; i < holds.size(); ++i) {
            if (!open[i]) continue;
            const int share = static_cast<int>(static_cast<long long>(remaining) * holds[i].ratio / ratioSum);
            const int room = holds[i].maxLb - out[i].weightLb;
            const int take = std::min(share, room);
            out[i].weightLb += take;
            given += take;
            if (take == room) {
                open[i] = false;
                closedOne = true;
            }
        }
        remaining -= given;
        if (closedOne) continue;
        // Only rounding is left: it goes to the open hold with the largest ratio.
        std::size_t best = holds.size();
        for (std::size_t i = 0; i < holds.size(); ++i) {
            if (open[i] && (best == holds.size() || holds[i].ratio > holds[best].ratio)) best = i;
        }
        if (best == holds.size()) break;
        const int take = std::min(remaining, holds[best].maxLb - out[best].weightLb);
        out[best].weightLb += take;
        remaining -= take;
        if (take == 0) open[best] = false;
    }
    return out;
}

bool isDaytime(int localHour) { return localHour >= 7 && localHour < 19; }

}  // namespace blocktime
