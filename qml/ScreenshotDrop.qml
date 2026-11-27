// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where a SimBrief airframe screenshot goes in (D-242): drag a PNG or JPEG onto it, paste one (Ctrl+V, after
// Win+Shift+S), or choose a file. Add Aircraft and the aircraft screen's New screenshot share it.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Shapes
import QtQuick.Dialogs
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Item {
    id: drop

    property string prompt: "Drop a screenshot of the airplane's SimBrief airframe page here"

    signal fileChosen(url file)
    signal pasteRequested()

    implicitHeight: Theme.dropAreaHeight

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: area.containsDrag ? Theme.accent : Theme.slotEmptyBorder
            strokeWidth: Theme.emptySlotBorderWidth
            strokeStyle: ShapePath.DashLine
            dashPattern: [Theme.dashLength, Theme.dashLength]
            fillColor: "transparent"

            PathRectangle {
                x: Theme.emptySlotBorderWidth / 2
                y: Theme.emptySlotBorderWidth / 2
                width: drop.width - Theme.emptySlotBorderWidth
                height: drop.height - Theme.emptySlotBorderWidth
                radius: Theme.radius
            }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 2 * Theme.spacing, implicitWidth)
        spacing: Theme.gap10

        Text {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: drop.width - 2 * Theme.spacing
            text: drop.prompt
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeFleetTitle
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: "or paste it (Ctrl+V)"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.gap8

            SecondaryButton {
                text: "Paste"
                onClicked: drop.pasteRequested()
            }

            SecondaryButton {
                text: "Choose file…"
                onClicked: picker.open()
            }
        }
    }

    DropArea {
        id: area

        anchors.fill: parent
        onDropped: event => {
            if (event.hasUrls && event.urls.length > 0) drop.fileChosen(event.urls[0]);
        }
    }

    Shortcut {
        enabled: drop.visible
        sequence: StandardKey.Paste
        onActivated: drop.pasteRequested()
    }

    FileDialog {
        id: picker

        title: "SimBrief airframe screenshot"
        nameFilters: ["Images (*.png *.jpg *.jpeg)"]
        onAccepted: drop.fileChosen(selectedFile)
    }
}
