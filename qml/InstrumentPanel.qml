// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The inset instrument panel below the dashboard's header (D-179): recessed into the page, darker than the cards
// around it, with a screw head in each corner. Children go inside its padding.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Rectangle {
    id: panel

    default property alias content: inner.data
    property int paddingTop: Theme.panelPaddingTop
    property int paddingSide: Theme.panelPaddingSide

    radius: Theme.panelRadius
    implicitHeight: inner.childrenRect.height + panel.paddingTop * 2
    gradient: Gradient {
        GradientStop { position: 0; color: Theme.panelInsetTop }
        GradientStop { position: 1; color: Theme.panelInsetBottom }
    }

    // The inner shadow along the top edge, and a faint light along the bottom: the panel sits below the page.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.panelInsetShadeHeight
        radius: panel.radius
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.panelInsetShade }
            GradientStop { position: 1; color: "transparent" }
        }
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: panel.radius
        anchors.rightMargin: panel.radius
        height: Theme.hairline
        color: Theme.panelEdgeLight
    }

    component Screw: Rectangle {
        width: Theme.screwSize
        height: Theme.screwSize
        radius: width / 2
        gradient: Gradient {
            GradientStop { position: 0; color: Theme.screwLight }
            GradientStop { position: 0.7; color: Theme.screwDark }
        }
        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 4
            height: 1.5
            rotation: 35
            color: Theme.screwSlot
        }
    }
    Screw { x: Theme.screwInset; y: Theme.screwInset }
    Screw { x: panel.width - width - Theme.screwInset; y: Theme.screwInset }
    Screw { x: Theme.screwInset; y: panel.height - height - Theme.screwInset }
    Screw { x: panel.width - width - Theme.screwInset; y: panel.height - height - Theme.screwInset }

    Item {
        id: inner

        x: panel.paddingSide
        y: panel.paddingTop
        width: panel.width - 2 * panel.paddingSide
        height: childrenRect.height
    }
}
