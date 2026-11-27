// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The aircraft screen (D-241, docs/fleet.md): the airplane as a character sheet on the left (its glyph, registration,
// status, lease month, New screenshot and Return airplane) and its values in four cards on the right. New screenshot
// shows only what would change before saving; Return asks first and shows the refund.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Layouts

Item {
    id: page

    required property AircraftModel model

    signal back()

    // 0 the cards, 1 New screenshot, 2 the return confirmation.
    property int panel: 0

    function show(registration) {
        page.panel = 0;
        return page.model.open(registration);
    }

    Connections {
        target: page.model

        function onReturned() { page.back(); }
    }

    component ValueCard: Rectangle {
        id: valueCard

        required property string title
        required property var card
        default property alias content: extra.data

        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        implicitHeight: cardColumn.implicitHeight + 2 * Theme.spacing
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        border.width: Theme.borderWidth

        ColumnLayout {
            id: cardColumn

            anchors.fill: parent
            anchors.margins: Theme.spacing
            spacing: Theme.gap6

            RowLayout {
                Layout.fillWidth: true

                Text {
                    Layout.fillWidth: true
                    text: valueCard.title.toUpperCase()
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    font.letterSpacing: Theme.labelLetterSpacing
                }

                Text {
                    text: valueCard.card.source ?? ""
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            Repeater {
                model: valueCard.card.rows ?? []

                RowLayout {
                    id: valueRow

                    required property var modelData

                    Layout.fillWidth: true

                    Text {
                        Layout.fillWidth: true
                        text: valueRow.modelData.label
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                    }

                    Text {
                        text: valueRow.modelData.value
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                    }
                }
            }

            ColumnLayout {
                id: extra

                Layout.fillWidth: true
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.pagePadding
        spacing: Theme.spacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.gap12

            SecondaryButton {
                text: "← Fleet"
                onClicked: page.back()
            }

            Text {
                Layout.fillWidth: true
                text: page.model.registration
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing

            // The airplane: the character.
            Rectangle {
                Layout.preferredWidth: Theme.fleetPanelWidth
                Layout.alignment: Qt.AlignTop
                implicitHeight: character.implicitHeight + 2 * Theme.spacing
                radius: Theme.radius
                color: Theme.surface
                border.color: Theme.border
                border.width: Theme.borderWidth

                ColumnLayout {
                    id: character

                    anchors.fill: parent
                    anchors.margins: Theme.spacing
                    spacing: Theme.gap12

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: Theme.portraitHeight
                        radius: Theme.radius
                        color: Theme.surfaceRaised
                        border.color: Theme.cardBorder
                        border.width: Theme.borderWidth

                        AirplaneGlyph {
                            anchors.centerIn: parent
                            kind: page.model.glyph
                            size: Theme.portraitGlyphSize
                        }
                    }

                    Text {
                        Layout.fillWidth: true
                        text: page.model.title
                        wrapMode: Text.WordWrap
                        color: Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeTitle
                        font.weight: Font.DemiBold
                    }

                    Text {
                        Layout.fillWidth: true
                        text: page.model.subtitle
                        wrapMode: Text.WordWrap
                        color: Theme.textSecondary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    Text {
                        text: page.model.statusText
                        color: page.model.glyph === "delivery" ? Theme.warn : Theme.good
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                        font.weight: Font.DemiBold
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.gap6

                        RowLayout {
                            Layout.fillWidth: true

                            Text {
                                Layout.fillWidth: true
                                text: "Lease month"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            Text {
                                text: page.model.leaseMonthText
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: Theme.progressBarHeight
                            radius: height / 2
                            color: Theme.surfaceRaised
                            border.color: Theme.border
                            border.width: Theme.borderWidth

                            Rectangle {
                                width: parent.width * page.model.leaseMonthProgress
                                height: parent.height
                                radius: parent.radius
                                color: Theme.accent
                            }
                        }
                    }

                    Text {
                        visible: text !== ""
                        Layout.fillWidth: true
                        text: page.model.blockedText
                        wrapMode: Text.WordWrap
                        color: Theme.warn
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }

                    SecondaryButton {
                        Layout.fillWidth: true
                        text: "New screenshot"
                        enabled: page.model.canReread
                        onClicked: {
                            page.model.cancelReread();
                            page.panel = 1;
                        }
                    }

                    SecondaryButton {
                        Layout.fillWidth: true
                        text: "Return airplane"
                        tone: Theme.bad
                        enabled: page.model.canReturn
                        onClicked: page.panel = 2
                    }
                }
            }

            // Its values: the gear.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacing

                GridLayout {
                    visible: page.panel === 0
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: Theme.spacing
                    rowSpacing: Theme.spacing

                    ValueCard {
                        title: "Layout"
                        card: page.model.layoutCard

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.gap4

                            Repeater {
                                model: page.model.holds

                                Rectangle {
                                    id: holdBar

                                    required property var modelData

                                    Layout.fillWidth: true
                                    Layout.preferredWidth: holdBar.modelData.share
                                    implicitHeight: Theme.holdBarHeight
                                    radius: Theme.radiusSmall
                                    color: Theme.surfaceRaised
                                    border.color: Theme.cardBorder
                                    border.width: Theme.borderWidth

                                    Text {
                                        anchors.centerIn: parent
                                        text: holdBar.modelData.name + " " + holdBar.modelData.lb
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                }
                            }
                        }
                    }

                    ValueCard {
                        title: "Weights"
                        card: page.model.weightsCard
                    }

                    ValueCard {
                        title: "SimBrief"
                        card: page.model.simbriefCard
                    }

                    ValueCard {
                        title: "Record & lease"
                        card: page.model.recordCard
                    }
                }

                // New screenshot: the drop area, then only what changes.
                ColumnLayout {
                    visible: page.panel === 1
                    Layout.fillWidth: true
                    spacing: Theme.gap12

                    ScreenshotDrop {
                        visible: page.model.rereadState === "idle" || page.model.rereadState === "failed"
                        Layout.fillWidth: true
                        prompt: "Drop " + page.model.registration + "'s SimBrief airframe screenshot here"
                        onFileChosen: file => page.model.readImageFile(file)
                        onPasteRequested: page.model.readClipboard()
                    }

                    Text {
                        visible: text !== ""
                        Layout.fillWidth: true
                        text: page.model.rereadState === "reading" ? "Claude is reading the screenshot…"
                            : page.model.rereadState === "failed" ? page.model.rereadError : ""
                        wrapMode: Text.WordWrap
                        color: page.model.rereadState === "failed" ? Theme.warn : Theme.textPrimary
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeBody
                    }

                    ValueCard {
                        visible: page.model.rereadState === "read"
                        title: "Changes from SimBrief"
                        card: ({ source: "nothing saved yet", rows: page.model.rereadChanges.map(
                                     c => ({ label: c.label, value: c.before + "  →  " + c.after })) })

                        Repeater {
                            model: page.model.rereadLayouts

                            SecondaryButton {
                                required property var modelData
                                required property int index

                                Layout.fillWidth: true
                                text: modelData.name + (index === page.model.rereadLayoutIndex ? "  ✓" : "")
                                enabled: modelData.enabled
                                onClicked: page.model.rereadLayoutIndex = index
                            }
                        }

                        Text {
                            visible: text !== ""
                            Layout.fillWidth: true
                            text: page.model.rereadRefusal
                            wrapMode: Text.WordWrap
                            color: Theme.warn
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignRight
                        spacing: Theme.gap8

                        SecondaryButton {
                            text: "Cancel"
                            onClicked: {
                                page.model.cancelReread();
                                page.panel = 0;
                            }
                        }

                        PrimaryButton {
                            visible: page.model.rereadState === "read"
                            text: "Save changes"
                            enabled: page.model.rereadRefusal === ""
                            onClicked: if (page.model.saveReread()) page.panel = 0
                        }
                    }
                }

                // Return: one confirmation, with the refund.
                Rectangle {
                    visible: page.panel === 2
                    Layout.fillWidth: true
                    implicitHeight: confirm.implicitHeight + 2 * Theme.spacing
                    radius: Theme.radius
                    color: Theme.surface
                    border.color: Theme.bad
                    border.width: Theme.borderWidth

                    ColumnLayout {
                        id: confirm

                        anchors.fill: parent
                        anchors.margins: Theme.spacing
                        spacing: Theme.gap10

                        Text {
                            text: "Return " + page.model.registration + "?"
                            color: Theme.textPrimary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeFleetTitle
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: page.model.returnText
                            wrapMode: Text.WordWrap
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeBody
                        }

                        RowLayout {
                            Layout.alignment: Qt.AlignRight
                            spacing: Theme.gap8

                            SecondaryButton {
                                text: "Keep it"
                                onClicked: page.panel = 0
                            }

                            SecondaryButton {
                                text: "Return " + page.model.registration
                                tone: Theme.bad
                                onClicked: page.model.returnPlane()
                            }
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }
}
