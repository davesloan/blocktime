// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "predictions.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace blocktime {

namespace {

TimePoint plus(TimePoint t, double s) {
    return t + std::chrono::duration_cast<TimePoint::duration>(std::chrono::duration<double>(s));
}

double secondsBetween(TimePoint from, TimePoint to) { return std::chrono::duration<double>(to - from).count(); }

}  // namespace

PlanTimeline::PlanTimeline(const std::vector<PlanFix>& fixes, const RouteProgress& route, int takeoffFuelLb) {
    if (fixes.empty() || fixes.back().elapsedS < 0 || fixes.back().fuelLb < 0) return;
    points_.push_back({0.0, 0.0, static_cast<double>(takeoffFuelLb), 0, ""});
    for (std::size_t k = 0; k < fixes.size(); ++k) {
        const PlanFix& f = fixes[k];
        if (f.elapsedS < 0 || f.fuelLb < 0) continue;  // a fix without the plan's values: interpolate across it
        points_.push_back({route.pointNm(k + 1), static_cast<double>(f.elapsedS), static_cast<double>(f.fuelLb),
                           f.iasKt, f.ident});
    }
}

template <class Field>
double PlanTimeline::along(double nm, Field field) const {
    if (nm <= points_.front().nm) return field(points_.front());
    if (nm >= points_.back().nm) return field(points_.back());
    const auto b = std::find_if(points_.begin(), points_.end(), [nm](const Point& p) { return p.nm >= nm; });
    const auto a = std::prev(b);
    if (b->nm <= a->nm) return field(*b);
    const double t = (nm - a->nm) / (b->nm - a->nm);
    return field(*a) + t * (field(*b) - field(*a));
}

double PlanTimeline::elapsedS(double nm) const {
    return empty() ? 0.0 : along(nm, [](const Point& p) { return p.elapsedS; });
}

double PlanTimeline::fuelLb(double nm) const {
    return empty() ? 0.0 : along(nm, [](const Point& p) { return p.fuelLb; });
}

int PlanTimeline::iasKt(double nm) const {
    const auto next = std::find_if(points_.begin(), points_.end(), [nm](const Point& p) { return p.nm > nm; });
    return next == points_.end() ? 0 : next->iasKt;
}

std::optional<double> PlanTimeline::fixNm(std::string_view ident) const {
    for (const Point& p : points_) {
        if (!p.ident.empty() && p.ident == ident) return p.nm;
    }
    return std::nullopt;
}

int PlanTimeline::enrouteS() const { return empty() ? 0 : static_cast<int>(points_.back().elapsedS); }
int PlanTimeline::landingFuelLb() const { return empty() ? 0 : static_cast<int>(points_.back().fuelLb); }

Predictions predict(const PlanTimeline& plan, TaxiTimes taxi, const FlightNow& f) {
    Predictions p;
    if (plan.empty()) return p;
    const double enroute = plan.enrouteS();
    const bool airborne = f.liftoff.has_value() && !f.touchdown;
    const double flown = airborne ? f.flownNm : (f.touchdown ? f.flownNm : 0.0);
    // What's left of each part of the flight; a part that runs long counts as done, never as negative time.
    double taxiOutLeft = 0, enrouteLeft = 0, taxiInLeft = taxi.inS;
    if (!f.liftoff && !f.touchdown) {
        taxiOutLeft = f.blockOut ? std::max(0.0, taxi.outS - secondsBetween(*f.blockOut, f.now)) : taxi.outS;
        enrouteLeft = enroute;
    } else if (airborne) {
        enrouteLeft = std::max(0.0, enroute - plan.elapsedS(flown));
    } else {
        taxiInLeft = std::max(0.0, taxi.inS - secondsBetween(*f.touchdown, f.now));
    }
    p.arrival = plus(f.now, taxiOutLeft + enrouteLeft + taxiInLeft);
    if (f.blockOut) {
        p.reference = plus(*f.blockOut, taxi.outS + enroute + taxi.inS);
        p.planLanding = plus(*f.blockOut, taxi.outS + enroute);
    }
    if (f.touchdown) return p;

    p.landing = plus(f.now, taxiOutLeft + enrouteLeft);
    const double here = plan.elapsedS(flown);
    const auto ahead = [&](std::string_view ident, std::optional<double>& nm, std::optional<TimePoint>& at) {
        const auto fix = plan.fixNm(ident);
        if (!fix || flown >= *fix) return;
        nm = *fix - flown;
        at = plus(f.now, taxiOutLeft + plan.elapsedS(*fix) - here);
    };
    ahead("TOC", p.tocNm, p.tocAt);
    ahead("TOD", p.todNm, p.todAt);

    p.planFuelLb = static_cast<int>(std::lround(plan.fuelLb(flown)));
    p.planLandingFuelLb = plan.landingFuelLb();
    p.fuelAtLandingLb = f.fuelLb - (*p.planFuelLb - *p.planLandingFuelLb);
    if (f.phase == "cruise") {
        if (const int ias = plan.iasKt(flown); ias > 0) p.cruiseSpeedKt = ias;
    }
    return p;
}

PunctualityView punctuality(TimePoint arrival, TimePoint reference) {
    const double late = secondsBetween(reference, arrival);
    const int minutes = static_cast<int>(std::lround(std::abs(late) / 60.0));
    if (minutes <= 2) return {Punctuality::OnTime, minutes};
    return {late > 0 ? Punctuality::Delayed : Punctuality::Early, minutes};
}

std::string punctualityText(const PunctualityView& p, bool final) {
    if (p.kind == Punctuality::OnTime) return "On time";
    return std::string(p.kind == Punctuality::Early ? "Early " : "Delayed ") + (final ? "" : "~") +
           std::to_string(p.minutes) + "m";
}

}  // namespace blocktime
