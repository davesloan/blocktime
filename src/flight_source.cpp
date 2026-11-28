// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_source.h"

void FlightSource::setFinishable(bool on) {
    if (on == finishable_) return;
    finishable_ = on;
    emit changed();
}
