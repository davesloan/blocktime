// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The in-flight panel (docs/dashboard.md "The in-flight panel", D-194..D-198, D-203; mockup
// docs/images/dashboard-inflight-v2.html): from block out to block in, the lamp strip, four steam gauges, the ATC card
// across the panel and the phase's readouts under it, four a row. Cards and gauges flip when they change, several at
// once in a wave from left to right (D-198). From InFlightModel.

pragma ComponentBehavior: Bound

import QtQuick
import BlockTime  // explicit, so hot reload from the source folder still sees the module (D-055)

InstrumentPanel {
    id: panel

    required property InFlightModel model

    property var waveQueue: []
    readonly property int readoutSlots: 14  // more than any phase shows (12), so cards persist and flip in place
    // The needles glide only while they can be seen (review 9.1): a gliding needle keeps the window drawing every frame
    // (about 4% of a core in a 1x replay), so the panel hidden, before block out or on another page, stops it. A
    // minimised window needs nothing: Qt stops drawing it.
    readonly property bool glide: panel.model.animate && panel.visible

    // A card's content changed: it flips in the wave, by its x across the panel, whatever the row (D-198).
    function requestFlip(card: Item): void {
        panel.waveQueue.push(card)
        Qt.callLater(panel.runWave)
    }
    function runWave(): void {
        const queue = panel.waveQueue
        panel.waveQueue = []
        queue.sort((a, b) => {
            const pa = a.mapToItem(panel, 0, 0)
            const pb = b.mapToItem(panel, 0, 0)
            return pa.x - pb.x || pa.y - pb.y
        })
        for (let i = 0; i < queue.length; ++i) queue[i].flip(i * Theme.flipWaveStepMs)
    }

    paddingTop: Theme.inflightPaddingTop
    paddingSide: Theme.inflightPaddingSide

    component Placard: Rectangle {
        id: placardBox

        property string text

        width: placardText.implicitWidth + 2 * Theme.gap8
        height: placardText.implicitHeight + Theme.gap4
        radius: Theme.gap2
        color: Theme.panelTitleBg

        Text {
            id: placardText

            anchors.centerIn: parent
            text: placardBox.text
            color: Theme.panelTitleText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizePanelTitle
            font.weight: Font.Bold
            font.letterSpacing: Theme.panelTitleSpacing
        }
    }

    Column {
        id: column

        width: parent.width
        spacing: Theme.inflightRowGap

        // The lamp strip (D-195).
        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.lampGap

            Repeater {
                model: 7

                Lamp {
                    id: lamp

                    required property int index
                    readonly property var look: panel.model.lamps[lamp.index] ?? ({})

                    name: lamp.look.name ?? ""
                    lens: lamp.look.lens ?? "amber"
                    known: lamp.look.known ?? false
                    lit: lamp.look.lit ?? false
                    wrong: lamp.look.wrong ?? false
                    flashOn: panel.model.flashOn
                }
            }
        }
        Rectangle {
            width: parent.width
            height: Theme.hairline
            color: Theme.lampStripRule
        }

        // The gauges (D-196), the speed and altitude gauges flipping when their faces change.
        Row {
            id: gauges

            readonly property real cell: width / 4

            width: parent.width

            Item {
                width: gauges.cell
                height: speedCard.height

                FlipCard {
                    id: speedCard

                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Theme.gaugeSize
                    key: panel.model.speedFace
                    payload: panel.model.speedFace
                    animate: panel.model.animate
                    onFlipRequested: card => panel.requestFlip(card)

                    SpeedGauge {
                        face: speedCard.shown ?? "gs"
                        speedKt: panel.model.speedKt
                        machText: panel.model.machText
                        bugKt: panel.model.speedBugKt
                        glide: panel.glide
                    }
                }
            }
            Item {
                width: gauges.cell
                height: altitudeCard.height

                FlipCard {
                    id: altitudeCard

                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Theme.gaugeSize
                    key: panel.model.altitudeFace
                    payload: panel.model.altitudeFace
                    animate: panel.model.animate
                    onFlipRequested: card => panel.requestFlip(card)

                    AltitudeGauge {
                        face: altitudeCard.shown ?? "altimeter"
                        altitudeFt: panel.model.altitudeFt
                        aglFt: panel.model.aglFt
                        setting: panel.model.altimeterSetting
                        glide: panel.glide
                    }
                }
            }
            Item {
                width: gauges.cell
                height: vsi.height

                VsiGauge {
                    id: vsi

                    anchors.horizontalCenter: parent.horizontalCenter
                    vsFpm: panel.model.vsFpm
                    glide: panel.glide
                }
            }
            Item {
                width: gauges.cell
                height: fuel.height

                FuelGauge {
                    id: fuel

                    anchors.horizontalCenter: parent.horizontalCenter
                    fuelLb: panel.model.fuelLb
                    scaleLb: panel.model.fuelScaleLb
                    lowLb: panel.model.lowFuelLb
                    text: panel.model.fuelText
                    bugLb: panel.model.planFuelLb
                    glide: panel.glide
                }
            }
        }

        // The ATC card (D-197) across the panel: ALTITUDE and APPROACH wide, SPEED narrow (D-203).
        Placard { text: "ATC" }
        Row {
            id: atc

            readonly property real unit: (width - 3 * Theme.readoutGap) / 4.2

            width: parent.width
            spacing: Theme.readoutGap

            Repeater {
                model: 4

                FlipCard {
                    id: slotCard

                    required property int index
                    readonly property var slot: panel.model.atcSlots[slotCard.index]

                    width: Theme.atcColumnWeights[slotCard.index] * atc.unit
                    visible: slotCard.slot !== undefined
                    key: slotCard.slot ? (slotCard.slot.title + "|" + slotCard.slot.value) : ""
                    payload: slotCard.slot
                    animate: panel.model.animate
                    onFlipRequested: card => panel.requestFlip(card)

                    Readout {
                        width: slotCard.width
                        opacity: slotCard.shown && !slotCard.shown.spacer ? 1 : 0  // SPEED's empty place on the ground
                        title: slotCard.shown?.title ?? ""
                        value: slotCard.shown?.value ?? ""
                        tag: slotCard.shown?.aside ?? ""
                        latest: slotCard.shown?.latest ?? false
                        words: true
                    }
                }
            }
        }

        // The phase's readouts (D-194): SOULS and FUEL REMAINING first, four a row, APPROACHES across two. After block
        // in, Settle Up on the right gives way to the summary once shut down and deboarded (0.15.1).
        Item {
            width: parent.width
            height: Math.max(phasePlacard.height, finishButton.visible ? finishButton.height : 0)

            Placard {
                id: phasePlacard

                anchors.verticalCenter: parent.verticalCenter
                text: panel.model.phaseTitle
            }
            PrimaryButton {
                id: finishButton

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: panel.model.finishable
                text: qsTr("Settle Up")
                onClicked: panel.model.finishFlight()
            }
        }
        Flow {
            id: readouts

            readonly property real unit: (width - (Theme.readoutColumns - 1) * Theme.readoutGap) / Theme.readoutColumns

            width: parent.width
            spacing: Theme.readoutGap

            Repeater {
                model: panel.readoutSlots

                FlipCard {
                    id: readoutCard

                    required property int index
                    readonly property var item: panel.model.readouts[readoutCard.index]

                    width: readoutCard.item?.wide ? 2 * readouts.unit + Theme.readoutGap : readouts.unit
                    visible: readoutCard.item !== undefined
                    // A value that ticks (fuel remaining) never flips (D-198).
                    key: readoutCard.item ? readoutCard.item.title + "|"
                                            + (readoutCard.item.ticks ? "" : readoutCard.item.value + "|" + readoutCard.item.stars)
                                          : ""
                    payload: readoutCard.item
                    animate: panel.model.animate
                    onFlipRequested: card => panel.requestFlip(card)

                    Readout {
                        width: readoutCard.width
                        title: readoutCard.shown?.title ?? ""
                        value: readoutCard.shown?.value ?? ""
                        unit: readoutCard.shown?.unit ?? ""
                        tag: readoutCard.shown?.tag ?? ""
                        flagged: readoutCard.shown?.flagged ?? false
                        words: readoutCard.shown?.words ?? false
                        stars: readoutCard.shown?.stars ?? -1
                    }
                }
            }
        }
    }
}
