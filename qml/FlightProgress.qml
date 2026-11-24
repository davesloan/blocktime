// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The flight's progress in design.md §13.5's six stages, each with its sub-events and their times (D-128). Built from
// the recorder's flight events by RecorderBridge.stages.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Card {
    id: progress

    required property RecorderBridge recorder

    title: "Flight progress"

    Repeater {
        model: progress.recorder.stages

        delegate: ColumnLayout {
            id: stage

            required property var modelData
            readonly property bool current: modelData.state === "current"
            readonly property bool done: modelData.state === "done"

            Layout.fillWidth: true
            spacing: 4

            RowLayout {
                spacing: 8

                Rectangle {
                    implicitWidth: Theme.stageDotSize
                    implicitHeight: Theme.stageDotSize
                    radius: Theme.stageDotSize / 2
                    color: stage.current ? Theme.accent : stage.done ? Theme.textSecondary : Theme.border
                }

                Text {
                    text: stage.modelData.name
                    color: stage.current ? Theme.textPrimary : stage.done ? Theme.textSecondary : Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                    font.weight: stage.current ? Font.DemiBold : Font.Normal
                }
            }

            Repeater {
                model: stage.modelData.lines

                delegate: RowLayout {
                    id: line

                    required property var modelData

                    Layout.fillWidth: true
                    Layout.leftMargin: Theme.stageDotSize + 8
                    spacing: 8

                    Text {
                        Layout.preferredWidth: Theme.stageTimeWidth
                        text: line.modelData.end !== "" ? line.modelData.start + " – " + line.modelData.end
                            : line.modelData.running ? line.modelData.start + " –" : line.modelData.start
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                        font.features: { "tnum": 1 }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: line.modelData.text
                        color: line.modelData.running ? Theme.textPrimary : Theme.textSecondary
                        wrapMode: Text.Wrap
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }
        }
    }
}
