// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The debrief of one flight (docs/debrief.md sections 5-6): Claude's review, or why it isn't there with a Retry
// button; the procedure checks; and the landing. Scrolls within the Flight Summary.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)
import QtQuick.Controls.Basic
import QtQuick.Layouts

ScrollView {
    id: panel

    // FlightListModel.summary(row).debrief, and the flight it's for.
    required property var debrief
    required property string flightId
    required property ReviewWriter reviews

    readonly property var review: panel.debrief.review ?? ({})
    readonly property string reviewState: panel.debrief.reviewState ?? ""

    clip: true
    contentWidth: availableWidth

    component SectionLabel: Text {
        Layout.fillWidth: true
        Layout.topMargin: Theme.gap8
        color: Theme.textSecondary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeLabel
        font.weight: Font.DemiBold
        font.letterSpacing: Theme.labelLetterSpacing
    }

    component Paragraph: Text {
        Layout.fillWidth: true
        visible: text !== ""
        wrapMode: Text.WordWrap
        color: Theme.textPrimary
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSizeBody
        lineHeight: 1.15
    }

    ColumnLayout {
        width: panel.availableWidth
        spacing: Theme.gap8

        SectionLabel { text: "DEBRIEF" }

        // Claude's review (D-161), or why it isn't there.
        Paragraph { text: panel.reviewState === "written" ? (panel.review.summary ?? "") : "" }

        Repeater {
            model: panel.reviewState === "written" ? (panel.review.workOn ?? []) : []

            Paragraph {
                required property string modelData
                required property int index

                text: (index + 1) + ". " + modelData
            }
        }

        Paragraph {
            text: panel.reviewState === "written" && (panel.review.landing ?? "") !== ""
                  ? "Landing: " + panel.review.landing : ""
        }

        Paragraph {
            text: panel.reviewState === "written" && (panel.review.atc ?? "") !== ""
                  ? "ATC (information): " + panel.review.atc : ""
        }

        RowLayout {
            Layout.fillWidth: true
            visible: panel.reviewState !== "written"
            spacing: Theme.gap12

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: panel.reviewState === "writing" ? "Writing the debrief…"
                    : panel.reviewState === "no_key" ? "Add a Claude API key to write debriefs"
                    : panel.reviewState === "failed" ? "Debrief not written: " + (panel.debrief.reviewError ?? "")
                    : "Debrief not written"
                color: Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeBody
            }

            SecondaryButton {
                // Not in a replay window: it has no ReviewWriter, and a replay calls nothing (D-175).
                visible: panel.reviews !== null && (panel.reviewState === "failed" || panel.reviewState === "missing")
                text: "Retry"
                onClicked: panel.reviews.retry(panel.flightId)
            }
        }

        SectionLabel { text: "CHECKS" }

        CheckList {
            Layout.fillWidth: true
            checks: panel.debrief.checks ?? []
        }

        SectionLabel { text: "LANDING" + (panel.debrief.starsText ? "  " + panel.debrief.starsText : "") }

        Repeater {
            model: panel.debrief.landingLines ?? []

            Text {
                required property string modelData
                required property int index

                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: modelData
                color: index === 0 ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontFamily
                font.pixelSize: index === 0 ? Theme.fontSizeBody : Theme.fontSizeSmall
            }
        }

        Text {
            visible: (panel.debrief.landingLines ?? []).length === 0
            text: "No touchdown was logged"
            color: Theme.textSecondary
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeBody
        }
    }
}
