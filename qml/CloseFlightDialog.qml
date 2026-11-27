// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Asked when the window is closed while a flight is open (review 1.9): closing ends the recording, and an
// interrupted flight is discarded (D-046, D-072).
Dialog {
    id: dialog

    signal closeAnyway

    // The flight has ended, but some of its events aren't written to the log yet (D-227).
    property bool eventsHeld: false

    modal: true
    closePolicy: Popup.CloseOnEscape
    anchors.centerIn: Overlay.overlay
    width: Math.min(Theme.dialogWidth, parent ? parent.width - 2 * Theme.pagePadding : Theme.dialogWidth)
    padding: Theme.pagePadding

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radius
        border.color: Theme.border
        border.width: Theme.borderWidth
    }

    Overlay.modal: Rectangle {
        color: "#99000000"
    }

    contentItem: ColumnLayout {
        spacing: Theme.gap12

        Text {
            text: dialog.eventsHeld ? "Events aren't written yet" : "A flight is open"
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeTitle
            font.weight: Font.DemiBold
        }

        Text {
            Layout.fillWidth: true
            text: dialog.eventsHeld
                  ? "Some of the flight's events couldn't be written to the log yet. Closing BlockTime now loses them, "
                    + "and the flight may never settle."
                  : "Closing BlockTime ends the recording. The flight is then interrupted and discarded: no money, no "
                    + "hours and no debrief."
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.gap4
            spacing: Theme.gap8

            Item {
                Layout.fillWidth: true
            }

            SecondaryButton {
                text: "Close and discard"
                tone: Theme.bad
                onClicked: {
                    dialog.close();
                    dialog.closeAnyway();
                }
            }

            PrimaryButton {
                text: "Keep flying"
                onClicked: dialog.close()
            }
        }
    }
}
