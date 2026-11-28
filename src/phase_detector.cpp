// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "phase_detector.h"

#include <algorithm>
#include <cmath>

#include "block_detector.h"  // kAirborneMinAglFt (D-042)
#include "geo.h"

namespace blocktime {

namespace {

constexpr double kMetersPerNm = 1852.0;

// A level's altitude from its samples' mean: the nearest thousand when within kLevelSnapFt of one (a slow capture
// drags the mean short of the level flown, D-133), else the nearest 100 ft.
int levelAltitude(double ft) {
    const double thousand = std::round(ft / 1000.0) * 1000.0;
    if (std::abs(ft - thousand) <= kLevelSnapFt) return static_cast<int>(thousand);
    return static_cast<int>(std::lround(ft / 100.0)) * 100;
}
int roundFt(double ft) { return static_cast<int>(std::lround(ft)); }

bool levelPhase(Phase p) {
    return p == Phase::Climb || p == Phase::Cruise || p == Phase::Descent || p == Phase::Approach ||
           p == Phase::GoAround;
}

}  // namespace

std::string_view markerCode(MarkerKind k) {
    switch (k) {
        case MarkerKind::Above10000: return "above_10000";
        case MarkerKind::TransitionAltitude: return "transition_altitude";
        case MarkerKind::Within200Nm: return "within_200nm";
        case MarkerKind::TransitionLevel: return "transition_level";
        case MarkerKind::Below10000: return "below_10000";
        case MarkerKind::FinalApproach: return "final_approach";
    }
    return "";
}

std::string_view phaseCode(Phase p) {
    switch (p) {
        case Phase::AtGate: return "at_gate";
        case Phase::Pushback: return "pushback";
        case Phase::TaxiOut: return "taxi_out";
        case Phase::Takeoff: return "takeoff";
        case Phase::Climb: return "climb";
        case Phase::Cruise: return "cruise";
        case Phase::Descent: return "descent";
        case Phase::Approach: return "approach";
        case Phase::GoAround: return "go_around";
        case Phase::Landing: return "landing";
        case Phase::TaxiIn: return "taxi_in";
        case Phase::Arrived: return "arrived";
    }
    return "";
}

PhaseDetector::PhaseDetector(PhasePlan plan, TimePoint blockOut, double altFt)
    : plan_(plan), started_{Phase::Pushback, Phase::AtGate, blockOut, roundFt(altFt)} {}

std::optional<int> PhaseDetector::levelFt() const {
    if (level_ && (phase_ == Phase::Climb || phase_ == Phase::Descent || phase_ == Phase::Approach))
        return level_->altFt;
    return std::nullopt;
}

PhaseChanged PhaseDetector::onBlockIn(TimePoint t, double altFt) {
    PhaseChanged e{Phase::Arrived, phase_, t, roundFt(altFt)};
    phase_ = Phase::Arrived;
    return e;
}

void PhaseDetector::change(std::vector<PhaseEvent>& out, Phase to, TimePoint t, int altFt) {
    out.push_back(PhaseChanged{to, phase_, t, altFt});
    phase_ = to;
}

double PhaseDetector::distanceToDestinationNm(const PhaseSample& s) const {
    return distanceMeters(s.lat, s.lon, plan_.destinationLat, plan_.destinationLon) / kMetersPerNm;
}

std::vector<PhaseEvent> PhaseDetector::onSample(const PhaseSample& s) {
    std::vector<PhaseEvent> out;
    advance(out, s);
    markers(out, s);
    previous_ = s;
    return out;
}

void PhaseDetector::advance(std::vector<PhaseEvent>& out, const PhaseSample& s) {
    const bool airborne = !s.onGround && s.aglFt >= kAirborneMinAglFt;

    switch (phase_) {
        case Phase::Pushback:
            if (s.parkingBrake) pushBrakeSet_ = true;
            if ((pushBrakeSet_ && !s.parkingBrake && s.groundSpeedKt >= kPushbackMoveKt) ||
                s.groundSpeedKt >= kTaxiWithoutPushKt) {
                change(out, Phase::TaxiOut, s.time, roundFt(s.altFt));
                taxi_.push_back(s);
            }
            return;
        case Phase::TaxiOut:
            onTaxiOut(out, s);
            return;
        case Phase::Takeoff:
            if (airborne && s.aglFt >= kClimbAglFt) change(out, Phase::Climb, s.time, roundFt(s.altFt));
            break;
        case Phase::Landing:
            if (s.onGround && s.groundSpeedKt < kTaxiInKt) change(out, Phase::TaxiIn, s.time, roundFt(s.altFt));
            return;
        default:
            break;
    }
    if (!levelPhase(phase_)) return;

    // Back on the ground after being airborne: the landing (the one rule that doesn't care which phase it was).
    if (s.onGround) {
        change(out, Phase::Landing, s.time, roundFt(s.altFt));
        window_.reset();
        level_.reset();
        descentHold_.reset();
        goAroundHold_.reset();
        step_.reset();
        leftCruiseAt_.reset();
        return;
    }
    if (!airborne) {
        window_.reset();
        return;
    }

    onAirborneLevels(out, s);
    if (phase_ == Phase::Climb || phase_ == Phase::Cruise) onDescentHold(out, s);

    if (phase_ == Phase::Cruise || phase_ == Phase::Descent) {
        // The first flaps below 10,000 ft start the approach; 3,000 ft AGL is the fallback (D-142).
        const bool flaps = s.flapsOut && s.altFt < kApproachBelowFt;
        if (flaps || s.aglFt < kApproachAglFt) {
            if (phase_ == Phase::Cruise) level_.reset();  // a cruise level isn't an approach level-off
            change(out, Phase::Approach, s.time, roundFt(s.altFt));
            approachLowestFt_ = s.altFt;
            step_.reset();
            leftCruiseAt_.reset();
            descentHold_.reset();
        }
    } else if (phase_ == Phase::Approach) {
        approachLowestFt_ = std::min(approachLowestFt_, s.altFt);
        if (s.vsFpm >= kGoAroundVsFpm && s.altFt >= approachLowestFt_ + kGoAroundGainFt) {
            if (!goAroundHold_) goAroundHold_ = Hold{s.activeS, s.time};
            if (s.activeS - goAroundHold_->startActiveS >= kGoAroundHoldS) {
                change(out, Phase::GoAround, goAroundHold_->start, roundFt(s.altFt));
                // The next approach gets its own 1,000 ft marker, so Live shows Short final again (D-149).
                fired_[static_cast<int>(MarkerKind::FinalApproach)] = false;
                goAroundHold_.reset();
                level_.reset();
                window_.reset();
            }
        } else {
            goAroundHold_.reset();
        }
    }
}

void PhaseDetector::onTaxiOut(std::vector<PhaseEvent>& out, const PhaseSample& s) {
    taxi_.push_back(s);
    while (!taxi_.empty() && taxi_.front().activeS < s.activeS - kTaxiOutMemoryS) taxi_.pop_front();
    if (!s.onGround || s.groundSpeedKt < kTakeoffRollKt) return;
    // The sim has no takeoff-roll signal: time the roll from where the steady acceleration began (D-124).
    std::size_t j = taxi_.size() - 1;
    while (j > 0 && taxi_[j - 1].groundSpeedKt <= taxi_[j].groundSpeedKt - kRollWalkBackKt) --j;
    change(out, Phase::Takeoff, taxi_[j].time, roundFt(s.altFt));
    taxi_.clear();
}

void PhaseDetector::onAirborneLevels(std::vector<PhaseEvent>& out, const PhaseSample& s) {
    if (!window_ || std::abs(s.altFt - window_->firstAltFt) > kLevelBandFt)
        window_ = Window{s.altFt, s.activeS, 0, 0, s.time};
    window_->sum += s.altFt;
    ++window_->count;

    if (level_) {
        if (std::abs(s.altFt - level_->altFt) > kLevelLeftFt) {
            if (phase_ == Phase::Cruise) {
                if (s.altFt > level_->altFt) {
                    out.push_back(StepClimb{s.time, level_->altFt});
                    step_ = level_->altFt;
                } else {
                    leftCruiseAt_ = s.time;  // top of descent or a step down: the descent hold decides
                }
            } else {
                out.push_back(LevelOffEnded{phase_, s.time, levelAltitude(level_->sum / level_->count)});
            }
            level_.reset();
        } else {
            level_->sum += s.altFt;
            ++level_->count;
        }
    }

    if (!level_ && s.activeS - window_->startActiveS >= kLevelHoldS) onLevelConfirmed(out, s);

    // Without a plan, a level held long enough above 10,000 ft is the cruise (recorder.md rule 9).
    if (level_ && phase_ == Phase::Climb && plan_.cruiseAltitudeFt <= 0 && level_->altFt >= kNoPlanCruiseMinFt &&
        s.activeS - level_->startActiveS >= kNoPlanCruiseHoldS) {
        out.push_back(LevelOffEnded{Phase::Climb, s.time, levelAltitude(level_->sum / level_->count)});
        change(out, Phase::Cruise, level_->start, level_->altFt);
        cruiseLevelFt_ = level_->altFt;
    }
}

void PhaseDetector::onLevelConfirmed(std::vector<PhaseEvent>& out, const PhaseSample& s) {
    const int alt = levelAltitude(window_->sum / window_->count);
    level_ = Level{alt, window_->start, window_->startActiveS, window_->sum, window_->count};
    switch (phase_) {
        case Phase::Climb:
            if (plan_.cruiseAltitudeFt > 0 && std::abs(alt - plan_.cruiseAltitudeFt) <= kCruiseWindowFt) {
                change(out, Phase::Cruise, window_->start, alt);
                cruiseLevelFt_ = alt;
            } else {
                out.push_back(LevelOff{Phase::Climb, window_->start, alt});
            }
            break;
        case Phase::Cruise:
            if (!step_ && leftCruiseAt_) {
                // A step down that levelled off before the descent hold could decide.
                out.push_back(StepClimb{*leftCruiseAt_, cruiseLevelFt_});
                step_ = cruiseLevelFt_;
            }
            if (step_) out.push_back(StepClimbEnded{window_->start, alt});
            step_.reset();
            leftCruiseAt_.reset();
            cruiseLevelFt_ = alt;
            break;
        case Phase::GoAround:
            change(out, Phase::Approach, window_->start, alt);
            approachLowestFt_ = s.altFt;
            out.push_back(LevelOff{Phase::Approach, window_->start, alt});
            break;
        default:  // Descent, Approach
            out.push_back(LevelOff{phase_, window_->start, alt});
            break;
    }
}

void PhaseDetector::onDescentHold(std::vector<PhaseEvent>& out, const PhaseSample& s) {
    if (s.vsFpm > kDescentVsFpm) {
        descentHold_.reset();
        return;
    }
    if (!descentHold_) descentHold_ = Hold{s.activeS, s.time};
    if (s.activeS - descentHold_->startActiveS < kDescentHoldS) return;

    const double rangeNm = kTodRangeFactor * 3.0 * s.altFt / 1000.0;
    if (phase_ == Phase::Climb || !plan_.hasDestination || distanceToDestinationNm(s) <= rangeNm) {
        change(out, Phase::Descent, descentHold_->start, roundFt(s.altFt));
        descentHold_.reset();
        level_.reset();
        window_.reset();
        step_.reset();
        leftCruiseAt_.reset();
    } else if (!step_) {
        out.push_back(StepClimb{leftCruiseAt_.value_or(descentHold_->start), cruiseLevelFt_});
        step_ = cruiseLevelFt_;
        leftCruiseAt_.reset();
    }
}

void PhaseDetector::markers(std::vector<PhaseEvent>& out, const PhaseSample& s) {
    if (!previous_) return;
    const PhaseSample& p = *previous_;
    const auto fire = [&](MarkerKind k, bool crossed) {
        bool& done = fired_[static_cast<int>(k)];
        if (done || !crossed) return;
        done = true;
        out.push_back(Marker{k, s.time, roundFt(s.altFt)});
    };
    const bool up = phase_ == Phase::Takeoff || phase_ == Phase::Climb || phase_ == Phase::Cruise;
    const bool down = phase_ == Phase::Cruise || phase_ == Phase::Descent || phase_ == Phase::Approach;
    const auto above = [&](double ft) { return p.altFt < ft && s.altFt >= ft; };
    const auto below = [&](double ft) { return p.altFt >= ft && s.altFt < ft; };

    fire(MarkerKind::Above10000, up && above(kMarkerAltFt));
    fire(MarkerKind::TransitionAltitude,
         up && plan_.transitionAltitudeFt > 0 && above(plan_.transitionAltitudeFt));
    // Airborne within 200 nm: crossing it on a longer flight, or the first airborne sample on a shorter one (D-256).
    if (plan_.hasDestination && !s.onGround && s.aglFt >= kAirborneMinAglFt) {
        fire(MarkerKind::Within200Nm, distanceToDestinationNm(s) <= kMarkerRangeNm);
    }
    fire(MarkerKind::TransitionLevel, down && plan_.transitionLevelFt > 0 && below(plan_.transitionLevelFt));
    fire(MarkerKind::Below10000, (phase_ == Phase::Descent || phase_ == Phase::Approach) && below(kMarkerAltFt));
    fire(MarkerKind::FinalApproach,
         phase_ == Phase::Approach && p.aglFt >= kFinalApproachAglFt && s.aglFt < kFinalApproachAglFt);
}

}  // namespace blocktime
