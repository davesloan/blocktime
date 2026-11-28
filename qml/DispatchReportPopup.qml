// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// View Dispatch Report (docs/dispatch.md, step 8; D-251): the accepted report over the dashboard, in a modal overlay.
// Re-import works until block out; after it the report is read-only (DispatchModel.locked).

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Popup {
    id: popup

    required property DispatchModel model

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent ? parent.width - 2 * Theme.pagePadding : 0, Theme.formWidth * 2)
    height: parent ? parent.height - 2 * Theme.pagePadding : 0
    padding: Theme.pagePadding
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // Cancelled from inside the popup: nothing left to show.
    Connections {
        target: popup.model

        function onChanged(): void {
            if (popup.model.reportTitle === "") popup.close()
        }
    }

    background: Rectangle {
        color: Theme.bg
        radius: Theme.radius
        border.color: Theme.border
        border.width: Theme.borderWidth
    }

    Overlay.modal: Rectangle {
        color: Theme.mapCreditBg
    }

    contentItem: ColumnLayout {
        spacing: Theme.gap12

        ScrollView {
            id: scroll

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            DispatchReport {
                width: scroll.availableWidth
                model: popup.model
            }
        }

        SecondaryButton {
            Layout.alignment: Qt.AlignRight
            text: "Close"
            onClicked: popup.close()
        }
    }
}
