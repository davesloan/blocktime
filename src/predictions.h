// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The predictions (docs/dashboard.md "The predictions", D-192, D-212): the arrival time and how it compares with the
// plan, top of climb and descent, landing, and the plan's fuel and speed where the airplane is. All of it is the
// plan's time from the current position along the route: the navlog's elapsed time and fuel at each fix, linear in
// distance between them, plus Dave's own taxi times.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "header_text.h"
#include "route_progress.h"
#include "simbrief.h"

namespace blocktime {

struct TaxiTimes {
    int outS = 0, inS = 0;
};

// The plan's elapsed time and fuel against the distance along the route.
class PlanTimeline {
public:
    // fixes[k] sits at route.pointNm(k + 1); the origin (0 NM) is 0 s and takeoffFuelLb. Empty when there are no
    // fixes or the last fix has no elapsed time or fuel (a plan with times but no fuel gets no predictions either).
    PlanTimeline(const std::vector<PlanFix>& fixes, const RouteProgress& route, int takeoffFuelLb);
    PlanTimeline() = default;

    bool empty() const { return points_.size() < 2; }
    double elapsedS(double nm) const;  // the plan's time from takeoff, clamped to the route
    double fuelLb(double nm) const;    // the plan's fuel on board, the same way
    int iasKt(double nm) const;        // the next fix's planned IAS; 0 = none
    std::optional<double> fixNm(std::string_view ident) const;  // the first fix with that ident
    int enrouteS() const;              // the last fix's elapsed time
    int landingFuelLb() const;         // the last fix's fuel

private:
    struct Point {
        double nm = 0, elapsedS = 0, fuelLb = 0;
        int iasKt = 0;
        std::string ident;
    };
    // Linear in distance between the points around `nm`.
    template <class Field>
    double along(double nm, Field field) const;
    std::vector<Point> points_;  // the origin, then the fixes with times and fuel
};

struct FlightNow {
    TimePoint now;
    std::optional<TimePoint> blockOut, liftoff, touchdown;
    double flownNm = 0;
    int fuelLb = 0;
    std::string phase;  // the recorder's code; "" before block out
};

struct Predictions {
    std::optional<TimePoint> arrival;    // at the gate
    std::optional<TimePoint> reference;  // from block out: block out + taxi out + enroute + taxi in (D-212)
    std::optional<double> tocNm, todNm;  // to go along the route; nullopt once passed or with no such fix
    std::optional<TimePoint> tocAt, todAt;
    std::optional<TimePoint> landing, planLanding;  // planLanding = reference − taxi in
    std::optional<int> planFuelLb;                  // the fuel gauge's bug, block out to touchdown
    std::optional<int> fuelAtLandingLb, planLandingFuelLb;
    std::optional<int> cruiseSpeedKt;               // the plan's IAS at the next fix, in the cruise phase only
};

// Nothing without a plan (an empty timeline).
Predictions predict(const PlanTimeline& plan, TaxiTimes taxi, const FlightNow& f);

enum class Punctuality { OnTime, Early, Delayed };
struct PunctualityView {
    Punctuality kind = Punctuality::OnTime;
    int minutes = 0;  // ≥ 0, rounded
};
// Within 2 minutes either way is on time (D-192).
PunctualityView punctuality(TimePoint arrival, TimePoint reference);
// "On time", "Early ~4m", "Delayed ~3m"; final (after block in): "Delayed 3m".
std::string punctualityText(const PunctualityView& p, bool final);

}  // namespace blocktime
