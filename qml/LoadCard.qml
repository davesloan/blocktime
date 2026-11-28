// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dispatch report's load card (D-234): the seats as a strip, the bags and holds, and zero fuel, takeoff and landing
// weight as tapes against their limits (the limit a red mark). Rolled at Dispatch, so it's full before import.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Card {
    id: card

    required property DispatchModel model

    title: card.model.loadTitle

    Row {
        Layout.fillWidth: true
        visible: card.model.seatSegments > 0
        spacing: Theme.gap4

        Repeater {
            model: card.model.seatSegments

            Rectangle {
                required property int index

                width: (parent.width - (card.model.seatSegments - 1) * Theme.gap4) / card.model.seatSegments
                height: Theme.seatStripHeight
                radius: Theme.gap4 - 1
                color: index < card.model.seatSegmentsFilled ? Theme.accent : Theme.border
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: card.model.loadText
        color: Theme.textSecondary
        wrapMode: Text.WordWrap
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
    }

    Repeater {
        model: card.model.weightBars

        RowLayout {
            id: tape

            required property var modelData

            Layout.fillWidth: true
            spacing: Theme.gap12

            Text {
                Layout.preferredWidth: Theme.weightLabelWidth
                text: tape.modelData.label
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: Theme.weightTapeHeight
                radius: height / 2
                color: Theme.bg

                Rectangle {
                    width: parent.width * tape.modelData.share
                    height: parent.height
                    radius: height / 2
                    color: Theme.accent
                }

                Rectangle {
                    visible: tape.modelData.limitShare > 0
                    x: parent.width * tape.modelData.limitShare - width / 2
                    y: -Theme.gap4
                    width: 2
                    height: parent.height + 2 * Theme.gap4
                    color: Theme.bad
                }
            }

            Item {
                Layout.preferredWidth: Theme.weightValueWidth
                implicitHeight: valueRow.implicitHeight

                Row {
                    id: valueRow

                    anchors.right: parent.right
                    spacing: Theme.gap6

                    Text {
                        text: tape.modelData.valueText
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                    }

                    Text {
                        visible: text !== ""
                        text: tape.modelData.limitText
                        color: Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                    }
                }
            }
        }
    }
}
