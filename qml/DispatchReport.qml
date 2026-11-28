// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dispatch report (docs/dispatch.md, D-234; board Final): the header with its badges, the money card (the ring,
// the split-flap board and the working), then the load and plan cards. Shown on Your Flight until Accept, and in the
// View Dispatch Report popup after.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

ColumnLayout {
    id: report

    required property DispatchModel model

    spacing: Theme.spacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.spacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.gap4

            Text {
                text: "DISPATCH REPORT"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLabel
                font.weight: Font.DemiBold
                font.letterSpacing: Theme.labelLetterSpacing
            }

            Text {
                text: report.model.reportTitle
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeReportTitle
                font.weight: Font.Bold
            }
        }

        // Extra fuel is information; the warnings are amber.
        Repeater {
            model: (report.model.extraFuelBadge !== "" ? [report.model.extraFuelBadge] : []).concat(report.model.warnings)

            Rectangle {
                id: badge

                required property string modelData
                required property int index

                readonly property bool warning: !(index === 0 && report.model.extraFuelBadge !== "")

                implicitWidth: badgeText.implicitWidth + 2 * Theme.gap12
                implicitHeight: Theme.reportBadgeHeight
                radius: height / 2
                color: badge.warning ? Theme.warnSurface : Theme.surfaceRaised
                border.color: badge.warning ? Theme.warn : Theme.slotEmptyBorder
                border.width: Theme.borderWidth

                Text {
                    id: badgeText

                    anchors.centerIn: parent
                    text: badge.modelData
                    color: badge.warning ? Theme.warn : Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeFeed
                    font.weight: Font.DemiBold
                }
            }
        }

        SecondaryButton {
            visible: !report.model.accepted && !report.model.locked
            text: "Change flight"
            onClicked: report.model.changeFlight()
        }
    }

    // Why an import was refused, or the runway note.
    Text {
        Layout.fillWidth: true
        visible: text !== ""
        text: report.model.message
        color: Theme.warn
        wrapMode: Text.WordWrap
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
    }

    // The money card: the ring, the board and the working.
    Rectangle {
        Layout.fillWidth: true
        implicitHeight: money.implicitHeight + 2 * Theme.pagePadding
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        border.width: Theme.borderWidth

        RowLayout {
            id: money

            anchors.fill: parent
            anchors.margins: Theme.pagePadding
            spacing: Theme.pagePadding

            MoneyRing {
                Layout.alignment: Qt.AlignTop
                slices: report.model.ring.slices
                selectedId: report.model.selectedId
                centerLabel: report.model.centerLabel
                centerValue: report.model.centerValue
                centerSub: report.model.centerSub
                centerTone: report.model.centerTone
                onPicked: id => report.model.select(id)
            }

            FlipBoard {
                Layout.alignment: Qt.AlignTop
                rows: report.model.boardRows
                importCount: report.model.importCount
                imported: report.model.imported
                selectedId: report.model.selectedId
                onPicked: id => report.model.select(id)
            }

            WorkingCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Theme.workingWidth
                model: report.model
            }
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2
        columnSpacing: Theme.spacing
        rowSpacing: Theme.spacing

        LoadCard {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            model: report.model
        }

        PlanCard {
            Layout.fillWidth: true
            Layout.preferredWidth: 1
            Layout.alignment: Qt.AlignTop
            model: report.model
        }
    }
}
