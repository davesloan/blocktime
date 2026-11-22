// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The live flight screen from UI test 1: the flight panel and the event feed.

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Item {
    id: page

    required property RecorderBridge recorder

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.spacing

        FlightPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            recorder: page.recorder
        }

        EventFeed {
            Layout.fillHeight: true
            Layout.preferredWidth: 360
            recorder: page.recorder
        }
    }
}
