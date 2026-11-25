// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The debrief's procedure checks (docs/debrief.md section 2): a mark, the check, what was measured, and the points a
// miss cost. A check that couldn't be measured shows its reason and costs nothing.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

ColumnLayout {
    id: checkList

    // FlightListModel.summary(row).debrief.checks: [{ label, detail, state, pointsText }]
    property var checks: []

    spacing: Theme.gap6

    Repeater {
        model: checkList.checks

        RowLayout {
            id: row

            required property var modelData

            Layout.fillWidth: true
            spacing: Theme.gap8

            Text {
                Layout.preferredWidth: Theme.checkMarkWidth
                text: row.modelData.state === "passed" ? "✓" : row.modelData.state === "missed" ? "✗" : "–"
                color: row.modelData.state === "passed" ? Theme.good
                     : row.modelData.state === "missed" ? Theme.bad : Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
            }

            Text {
                Layout.fillWidth: true
                text: row.modelData.label
                color: row.modelData.state === "not_checked" ? Theme.textSecondary : Theme.textPrimary
                elide: Text.ElideRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            Text {
                Layout.fillWidth: true
                text: row.modelData.detail
                color: Theme.textSecondary
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            Text {
                Layout.preferredWidth: Theme.checkPointsWidth
                text: row.modelData.pointsText
                color: Theme.bad
                horizontalAlignment: Text.AlignRight
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.features: { "tnum": 1 }
            }
        }
    }
}
