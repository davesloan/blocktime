// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Add Aircraft (D-242, D-245, docs/fleet.md): the screenshot, then what Claude read (fields Dave can fix), the type
// and layout with the hold split, the flight number, the delivery (factory or instant, with its money lines) and the
// lease. Lease is greyed with the reason while any check fails.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: page

    required property AddAircraftModel model

    signal back()

    component Section: Rectangle {
        id: section

        required property string number
        required property string title
        property string note
        default property alias content: body.data

        Layout.fillWidth: true
        implicitHeight: column.implicitHeight + 2 * Theme.spacing
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.border
        border.width: Theme.borderWidth

        ColumnLayout {
            id: column

            anchors.fill: parent
            anchors.margins: Theme.spacing
            spacing: Theme.gap10

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.gap8

                Text {
                    text: section.number
                    color: Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.Bold
                }

                Text {
                    Layout.fillWidth: true
                    text: section.title.toUpperCase()
                    color: Theme.textSecondary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLabel
                    font.weight: Font.DemiBold
                    font.letterSpacing: Theme.labelLetterSpacing
                }

                Text {
                    text: section.note
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeSmall
                }
            }

            ColumnLayout {
                id: body

                Layout.fillWidth: true
                spacing: Theme.gap6
            }
        }
    }

    component Choice: Rectangle {
        id: choice

        required property string title
        property string detail
        property bool selected
        property bool selectable: true
        default property alias content: extra.data

        signal picked()

        Layout.fillWidth: true
        implicitHeight: choiceColumn.implicitHeight + 2 * Theme.gap10
        radius: Theme.radiusSmall
        color: choice.selected ? Theme.surfaceRaised : "transparent"
        border.color: choice.selected ? Theme.accent : Theme.border
        border.width: Theme.borderWidth
        opacity: choice.selectable ? 1 : 0.45

        MouseArea {
            anchors.fill: parent
            enabled: choice.selectable
            cursorShape: Qt.PointingHandCursor
            onClicked: choice.picked()
        }

        ColumnLayout {
            id: choiceColumn

            anchors.fill: parent
            anchors.margins: Theme.gap10
            spacing: Theme.gap4

            Text {
                Layout.fillWidth: true
                text: choice.title
                wrapMode: Text.WordWrap
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
                font.weight: Font.DemiBold
            }

            Text {
                visible: choice.detail !== ""
                Layout.fillWidth: true
                text: choice.detail
                wrapMode: Text.WordWrap
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            ColumnLayout {
                id: extra

                Layout.fillWidth: true
                spacing: Theme.gap4
            }
        }
    }

    component AmountRow: RowLayout {
        id: amountRow

        required property string label
        required property string amount
        property bool strong

        Layout.fillWidth: true

        Text {
            Layout.fillWidth: true
            text: amountRow.label
            color: amountRow.strong ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: amountRow.strong ? Font.DemiBold : Font.Normal
        }

        Text {
            text: amountRow.amount
            color: Theme.textPrimary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
            font.weight: amountRow.strong ? Font.DemiBold : Font.Normal
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
                text: "Add Aircraft"
                color: Theme.textPrimary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeTitle
                font.weight: Font.DemiBold
            }
        }

        ScreenshotDrop {
            visible: page.model.state === "empty"
            Layout.fillWidth: true
            Layout.fillHeight: true
            onFileChosen: file => page.model.readImageFile(file)
            onPasteRequested: page.model.readClipboard()
        }

        // The screenshot's status once one is in.
        Rectangle {
            visible: page.model.state !== "empty"
            Layout.fillWidth: true
            implicitHeight: status.implicitHeight + 2 * Theme.gap10
            radius: Theme.radius
            color: Theme.surface
            border.color: page.model.state === "failed" ? Theme.warn : Theme.border
            border.width: Theme.borderWidth

            RowLayout {
                id: status

                anchors.fill: parent
                anchors.margins: Theme.gap10
                spacing: Theme.gap12

                Text {
                    Layout.fillWidth: true
                    text: page.model.state === "reading" ? "Claude is reading the screenshot…"
                        : page.model.state === "read" ? "✓ Read by Claude. Check each value against SimBrief."
                        : page.model.readError
                    wrapMode: Text.WordWrap
                    color: page.model.state === "failed" ? Theme.warn : Theme.textPrimary
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeBody
                }

                SecondaryButton {
                    visible: page.model.canRetry
                    text: "Retry"
                    onClicked: page.model.retry()
                }

                SecondaryButton {
                    text: "Use another screenshot"
                    onClicked: page.model.clear()
                }
            }
        }

        Flickable {
            id: form

            visible: page.model.state !== "empty"
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: columns.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            RowLayout {
                id: columns

                width: form.width
                spacing: Theme.spacing

                // 1. What SimBrief says.
                Section {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 1
                    number: "1"
                    title: "From SimBrief"
                    note: "must match SimBrief"

                    Repeater {
                        model: page.model.fields

                        RowLayout {
                            id: field

                            required property var modelData

                            Layout.fillWidth: true
                            spacing: Theme.gap10

                            Text {
                                Layout.preferredWidth: Theme.formLabelWidth
                                text: field.modelData.label
                                color: field.modelData.missing ? Theme.warn : Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody
                            }

                            TextInputField {
                                Layout.fillWidth: true
                                text: field.modelData.value
                                placeholderText: field.modelData.missing ? "not read" : ""
                                onEditingFinished: page.model.setField(field.modelData.key, text)
                            }

                            Text {
                                Layout.preferredWidth: Theme.formUnitWidth
                                text: field.modelData.unit
                                color: Theme.textMuted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 1
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    // 2. The type and layout.
                    Section {
                        number: "2"
                        title: "Type and layout"
                        note: "aircraft files"

                        Repeater {
                            model: page.model.checks

                            Text {
                                required property var modelData

                                Layout.fillWidth: true
                                text: (modelData.ok ? "✓  " : "✗  ") + modelData.text
                                wrapMode: Text.WordWrap
                                color: modelData.ok ? Theme.textPrimary : Theme.warn
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody
                            }
                        }

                        Repeater {
                            model: page.model.layouts

                            Choice {
                                required property var modelData
                                required property int index

                                title: modelData.name
                                detail: modelData.detail
                                selectable: modelData.enabled
                                selected: index === page.model.layoutIndex
                                onPicked: page.model.layoutIndex = index
                            }
                        }

                        Text {
                            visible: text !== ""
                            Layout.fillWidth: true
                            text: page.model.holdsText
                            wrapMode: Text.WordWrap
                            color: Theme.textSecondary
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.gap4

                            Repeater {
                                model: page.model.holds

                                Rectangle {
                                    required property var modelData

                                    Layout.fillWidth: true
                                    Layout.preferredWidth: modelData.share
                                    implicitHeight: Theme.holdBarHeight
                                    radius: Theme.radiusSmall
                                    color: Theme.surfaceRaised
                                    border.color: Theme.cardBorder
                                    border.width: Theme.borderWidth

                                    Text {
                                        anchors.centerIn: parent
                                        text: parent.modelData.name + " " + parent.modelData.lb
                                        color: Theme.textSecondary
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSizeSmall
                                    }
                                }
                            }
                        }
                    }

                    // 3. Dave's own.
                    Section {
                        number: "3"
                        title: "Yours"

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.gap10

                            Text {
                                Layout.preferredWidth: Theme.formLabelWidth
                                text: "Flight number"
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeBody
                            }

                            TextInputField {
                                Layout.preferredWidth: Theme.shortFieldWidth
                                text: page.model.flightNumber
                                placeholderText: "optional"
                                onEditingFinished: page.model.flightNumber = text
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    Layout.preferredWidth: 1
                    Layout.fillWidth: true
                    spacing: Theme.spacing

                    // 4. Where it starts.
                    Section {
                        number: "4"
                        title: "Delivery"

                        Choice {
                            title: page.model.factoryText
                            detail: "Its first flight is the empty delivery flight."
                            selected: page.model.delivery === "factory"
                            onPicked: page.model.delivery = "factory"
                        }

                        Choice {
                            title: "Instant delivery"
                            detail: "Delivered at once to an airport you choose."
                            selected: page.model.delivery === "instant"
                            onPicked: page.model.delivery = "instant"

                            TextInputField {
                                visible: page.model.delivery === "instant"
                                Layout.preferredWidth: Theme.shortFieldWidth
                                text: page.model.deliveryIcao
                                placeholderText: "ICAO"
                                onEditingFinished: page.model.deliveryIcao = text
                            }

                            Text {
                                visible: text !== ""
                                Layout.fillWidth: true
                                text: page.model.deliveryCheck
                                wrapMode: Text.WordWrap
                                color: Theme.textSecondary
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                            }

                            MoneyLines {
                                visible: page.model.deliveryLines.length > 0
                                Layout.fillWidth: true
                                costs: page.model.deliveryLines
                                totalText: page.model.deliveryTotalText
                            }
                        }
                    }

                    // 5. The money.
                    Section {
                        number: "5"
                        title: "Lease"

                        Repeater {
                            model: page.model.leaseLines

                            AmountRow {
                                required property var modelData

                                label: modelData.label
                                amount: modelData.amount
                            }
                        }

                        AmountRow {
                            visible: page.model.cashAfterText !== ""
                            label: "Cash after"
                            amount: page.model.cashAfterText
                            strong: true
                        }
                    }
                }
            }
        }

        // The footer: why Lease is greyed, or the lease itself.
        RowLayout {
            visible: page.model.state !== "empty"
            Layout.fillWidth: true
            spacing: Theme.gap12

            Text {
                Layout.fillWidth: true
                text: page.model.refusal !== "" ? page.model.refusal
                    : "Charged today. Returning it later refunds the unused days of the month."
                wrapMode: Text.WordWrap
                color: page.model.refusal !== "" ? Theme.warn : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            SecondaryButton {
                text: "Cancel"
                onClicked: page.back()
            }

            PrimaryButton {
                text: page.model.leaseButtonText
                enabled: page.model.refusal === ""
                onClicked: page.model.lease()
            }
        }
    }
}
