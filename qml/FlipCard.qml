// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A card that flips when its content changes (D-198): a half-turn about its horizontal axis with a little
// perspective, the new content showing from the midpoint, like a flip clock. The content draws `shown`, which follows
// `payload` at once unless the key (the title and value, not a ticking time) changed while `animate`: then the card
// asks the panel for its place in the wave, and `shown` changes at the flip's midpoint. A burst of changes in one
// update is one check (Qt.callLater), so a replay seek never flips.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Item {
    id: card

    property string key
    property var payload
    property var shown
    property bool animate: false
    default property alias content: inner.data

    property string shownKey
    property bool started: false
    property bool pending: false  // waiting for, or in, a flip

    signal flipRequested(Item card)

    function sync(): void {
        if (card.pending) return
        if (card.started && card.animate && card.key !== card.shownKey) {
            card.pending = true
            card.flipRequested(card)
            return
        }
        card.shown = card.payload
        card.shownKey = card.key
        card.started = true
    }

    // Starts the flip after `delayMs` (the wave's step times its place).
    function flip(delayMs: int): void {
        pause.duration = delayMs
        turning.restart()
    }

    implicitHeight: inner.childrenRect.height
    transform: Rotation {
        id: turn

        origin.x: card.width / 2
        origin.y: card.height / 2
        axis { x: 1; y: 0; z: 0 }
        angle: 0
    }

    onPayloadChanged: Qt.callLater(card.sync)
    onKeyChanged: Qt.callLater(card.sync)
    Component.onCompleted: card.sync()

    SequentialAnimation {
        id: turning

        PauseAnimation { id: pause }
        NumberAnimation {
            target: turn
            property: "angle"
            from: 0
            to: 90
            duration: Theme.flipDurationMs / 2
            easing.type: Easing.InQuad
        }
        ScriptAction {
            script: {
                card.shown = card.payload
                card.shownKey = card.key
            }
        }
        NumberAnimation {
            target: turn
            property: "angle"
            from: -90
            to: 0
            duration: Theme.flipDurationMs / 2
            easing.type: Easing.OutQuad
        }
        ScriptAction {
            script: {
                card.pending = false
                card.sync()  // it may have changed again during the flip
            }
        }
    }

    Item {
        id: inner

        width: card.width
        height: childrenRect.height
    }
}
