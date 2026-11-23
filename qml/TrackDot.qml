// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A start or end marker on a flown track: a filled circle with a dark ring, so it reads on any map colour.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Rectangle {
    property color tone: Theme.accent

    width: 14
    height: 14
    radius: 7
    color: tone
    border.color: Theme.bg
    border.width: 3
}
