// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "your_flight_stage.h"

namespace blocktime {

YourFlightStage yourFlightStage(const StageInputs& in) {
    using S = YourFlightStage;
    if (in.flightOpen) return S::Flying;
    if (in.settlePending || (in.summaryShowing && in.finishable)) return S::Arrived;
    if (in.summaryShowing) return S::Summary;
    if (in.formOpen && !in.accepted) return S::Form;
    if (in.accepted) return S::Accepted;
    if (in.planImported) return S::Imported;
    if (in.planned) return S::Dispatched;
    if (in.formOpen) return S::Form;
    return S::Empty;
}

std::string_view stageName(YourFlightStage stage) {
    switch (stage) {
        case YourFlightStage::Empty: return "empty";
        case YourFlightStage::Form: return "form";
        case YourFlightStage::Dispatched: return "dispatched";
        case YourFlightStage::Imported: return "imported";
        case YourFlightStage::Accepted: return "accepted";
        case YourFlightStage::Flying: return "flying";
        case YourFlightStage::Arrived: return "arrived";
        case YourFlightStage::Summary: return "summary";
    }
    return "empty";
}

}  // namespace blocktime
