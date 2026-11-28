// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dispatch report's split-flap board (D-234): a row of 20 tiles per money line, fares at the top and profit at the
// bottom, from DispatchModel.boardRows. Labels light; values settle in amber (costs) or green (fares, profit). Each
// row is a button that selects its line. The flip runs once per import, row by row, and only for the rows marked
// changed (every row at the first import); each row's timer stops when it has settled.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

Rectangle {
    id: board

    required property var rows       // [{id, text, valueStart, tone, changed}]
    required property int importCount
    required property bool imported
    required property string selectedId

    signal picked(string id)

    // After an import the rows are rebuilt; the changed ones then flip in, one after another.
    function startFlips(): void {
        let order = 0
        for (let i = 0; i < rowRepeater.count; ++i) {
            const item = rowRepeater.itemAt(i) as FlipBoardRow
            if (item && item.modelData.changed) item.startFlip(order++)
        }
    }

    onImportCountChanged: Qt.callLater(board.startFlips)

    implicitWidth: rowColumn.implicitWidth + 2 * Theme.gap12
    implicitHeight: rowColumn.implicitHeight + 2 * Theme.gap12
    radius: Theme.radius
    color: Theme.boardBg

    component FlipBoardRow: Rectangle {
        id: row

        required property var modelData
        required property int index
        required property bool imported
        required property bool selected

        // The flip: -1 settled; else the tick, counting up from minus this row's delay.
        property int tick: -1
        readonly property string flapChars: "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$-,"

        signal picked()

        function startFlip(order: int): void {
            row.tick = -order * Theme.flapRowDelay
            flapTimer.start()
        }

        function settled(column: int): bool {
            return row.tick < 0 || row.tick >= column * Theme.flapColumnDelay
        }

        function charAt(column: int): string {
            const ch = row.modelData.text.charAt(column)
            if (row.settled(column)) return ch
            return row.flapChars.charAt((row.tick * 7 + column * 13 + row.index * 5) % row.flapChars.length)
        }

        function colourAt(column: int): color {
            if (!row.imported) return Theme.tileWaiting
            if (!row.settled(column) || column < row.modelData.valueStart) return Theme.tileLabel
            const tone = row.modelData.tone
            return tone === "income" || tone === "profit" ? Theme.tileGood
                 : tone === "loss" ? Theme.tileLoss
                 : Theme.tileCost
        }

        implicitWidth: tiles.implicitWidth + 2 * Theme.gap4
        implicitHeight: tiles.implicitHeight + 2 * Theme.gap4
        radius: Theme.radiusSmall
        color: "transparent"
        border.width: Theme.borderWidth
        border.color: row.selected ? Theme.accent : rowHover.hovered ? Theme.boardRowHover : "transparent"

        Timer {
            id: flapTimer

            interval: Theme.flapTickMs
            repeat: true
            onTriggered: {
                row.tick += 1
                if (row.tick > 20 * Theme.flapColumnDelay) {
                    row.tick = -1
                    stop()
                }
            }
        }

        Row {
            id: tiles

            anchors.centerIn: parent
            spacing: Theme.boardTileGap

            Repeater {
                model: 20

                Rectangle {
                    id: tile

                    required property int index

                    width: Theme.boardTileWidth
                    height: Theme.boardTileHeight
                    radius: Theme.gap4 - 1
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Theme.tileTop }
                        GradientStop { position: 0.5; color: Theme.tileTop }
                        GradientStop { position: 0.501; color: Theme.tileBottom }
                        GradientStop { position: 1.0; color: Theme.tileBottom }
                    }

                    Text {
                        anchors.centerIn: parent
                        text: row.charAt(tile.index)
                        color: row.colourAt(tile.index)
                        font.family: Theme.boardFontFamily
                        font.pixelSize: Theme.fontSizeBoardTile
                        font.weight: Font.Bold
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: Theme.hairline
                        color: Theme.tileSeam
                    }
                }
            }
        }

        HoverHandler {
            id: rowHover

            cursorShape: Qt.PointingHandCursor
        }

        TapHandler {
            onTapped: row.picked()
        }

        Accessible.role: Accessible.Button
        Accessible.name: row.modelData.text
    }

    Column {
        id: rowColumn

        anchors.centerIn: parent
        spacing: Theme.boardRowGap

        Repeater {
            id: rowRepeater

            model: board.rows

            FlipBoardRow {
                id: boardRow

                imported: board.imported
                selected: board.selectedId === boardRow.modelData.id
                onPicked: board.picked(boardRow.modelData.id)
            }
        }
    }
}
