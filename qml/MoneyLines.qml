// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Money lines, label left and amount right in tabular figures, then the total: New Flight's preview and the
// planned flight's estimate (design rule 1).

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

ColumnLayout {
    id: lines

    // Each entry is { category, label, amount, working } (moneyLinesToQml), amount already formatted ("$28,150",
    // "-$660").
    property var income: []
    property var costs: []
    property string totalText
    property string fuelNote  // "+ fuel" while the fuel cost isn't known

    spacing: Theme.gap4

    component Line: RowLayout {
        id: line

        required property var modelData

        Layout.fillWidth: true
        spacing: Theme.gap12

        Text {
            Layout.fillWidth: true
            text: line.modelData.label
            color: Theme.textSecondary
            elide: Text.ElideRight
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }

        Text {
            text: line.modelData.amount
            color: line.modelData.amount.startsWith("-") ? Theme.textPrimary : Theme.good
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.features: { "tnum": 1 }
        }
    }

    Repeater {
        model: lines.income

        Line {}
    }

    Repeater {
        model: lines.costs

        Line {}
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.topMargin: Theme.gap4
        Layout.bottomMargin: Theme.gap2
        implicitHeight: Theme.hairline
        color: Theme.border
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.gap12

        Text {
            Layout.fillWidth: true
            text: lines.fuelNote === "" ? "Total" : "Total " + lines.fuelNote
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.DemiBold
        }

        Text {
            text: lines.totalText
            color: lines.totalText.startsWith("-") ? Theme.bad : Theme.good
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
        }
    }
}
