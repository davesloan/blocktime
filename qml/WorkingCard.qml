// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dispatch report's working column (D-234): a hint until a line is picked, then the line, its amount, how it was
// worked out and what changed since the last import; Accept flight and Cancel flight at the bottom.

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

ColumnLayout {
    id: working

    required property DispatchModel model

    spacing: Theme.gap12

    // Before a selection: what to do next.
    Rectangle {
        Layout.fillWidth: true
        visible: working.model.selectedId === ""
        implicitHeight: hintText.implicitHeight + 2 * Theme.spacing
        radius: Theme.radius
        color: "transparent"
        border.color: Theme.border
        border.width: Theme.borderWidth

        Text {
            id: hintText

            anchors.fill: parent
            anchors.margins: Theme.spacing
            text: working.model.hint
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            lineHeight: 1.4
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }
    }

    // The selected line and its working.
    Rectangle {
        id: selected

        readonly property var line: working.model.selection

        Layout.fillWidth: true
        visible: working.model.selectedId !== ""
        implicitHeight: selectedColumn.implicitHeight + 2 * Theme.spacing
        radius: Theme.radius
        color: Theme.bg
        border.color: Theme.accent
        border.width: Theme.borderWidth

        ColumnLayout {
            id: selectedColumn

            anchors.fill: parent
            anchors.margins: Theme.spacing
            spacing: Theme.gap8

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: selected.line.label ?? ""
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.Bold
                }

                Text {
                    text: selected.line.amountText ?? ""
                    color: selected.line.tone === "income" || selected.line.tone === "profit" ? Theme.incomeText
                                                                                             : Theme.costText
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeTitle
                    font.weight: Font.Bold
                }
            }

            Repeater {
                model: selected.line.lines ?? []

                Text {
                    required property string modelData

                    Layout.fillWidth: true
                    text: modelData
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }
            }

            Text {
                Layout.fillWidth: true
                visible: text !== ""
                text: selected.line.note ?? ""
                color: Theme.noteText
                wrapMode: Text.WordWrap
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeFeed
            }
        }
    }

    Item {
        Layout.fillHeight: true
    }

    Button {
        id: acceptButton

        Layout.fillWidth: true
        visible: !working.model.accepted
        enabled: working.model.imported && !working.model.locked && !working.model.busy
        text: "Accept flight"
        opacity: enabled ? 1 : 0.35
        onClicked: working.model.accept()

        contentItem: Text {
            text: acceptButton.text
            color: Theme.textOnGood
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeAccept
            font.weight: Font.ExtraBold
        }

        background: Rectangle {
            implicitHeight: Theme.acceptHeight
            radius: Theme.radius
            color: acceptButton.down ? Qt.darker(Theme.good, 1.2)
                 : acceptButton.hovered ? Qt.lighter(Theme.good, 1.1)
                 : Theme.good
        }
    }

    SecondaryButton {
        Layout.fillWidth: true
        visible: !working.model.locked
        text: "Cancel flight"
        tone: Theme.bad
        onClicked: confirmCancel.open()

        // One click below Accept shouldn't lose the flight, its loads and its plan: ask first.
        Dialog {
            id: confirmCancel

            parent: Overlay.overlay
            anchors.centerIn: parent
            modal: true
            padding: Theme.pagePadding

            background: Rectangle {
                color: Theme.surface
                radius: Theme.radius
                border.color: Theme.border
                border.width: Theme.borderWidth
            }

            contentItem: ColumnLayout {
                spacing: Theme.spacing

                Text {
                    text: "Cancel " + working.model.reportTitle + "? It's free."
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }

                RowLayout {
                    Layout.alignment: Qt.AlignRight
                    spacing: Theme.gap8

                    SecondaryButton {
                        text: "Keep it"
                        onClicked: confirmCancel.close()
                    }

                    PrimaryButton {
                        text: "Cancel flight"
                        onClicked: {
                            confirmCancel.close();
                            working.model.cancelFlight();
                        }
                    }
                }
            }
        }
    }
}
