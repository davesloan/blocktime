// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one place colours, sizes and fonts are written (D-054). Dark only.

pragma Singleton

import QtQuick

QtObject {
    // Surfaces, darkest first
    readonly property color bg: "#0E1116"
    readonly property color surface: "#161B22"
    readonly property color surfaceRaised: "#1C222B"
    readonly property color surfaceHover: "#1A1F27"  // between surface and surfaceRaised
    readonly property color border: "#262D37"
    readonly property color mapCreditBg: "#B30E1116"  // bg at 70%, behind the map's credit line

    // Text
    readonly property color textPrimary: "#E6EAF0"
    readonly property color textSecondary: "#8B95A3"
    readonly property color textMuted: "#5B6573"  // stages not reached yet

    // Meaning
    readonly property color accent: "#3B82F6"
    // Text on an accent fill. (Not "onAccent": QML reads on<Capital> as a signal handler.)
    readonly property color textOnAccent: "#FFFFFF"
    readonly property color good: "#22C55E"
    readonly property color warn: "#F59E0B"
    readonly property color bad: "#EF4444"
    readonly property color warnSurface: "#2A2112"  // warn mixed into bg: the Not recording banner
    readonly property color goodSurface: "#132A1C"  // good mixed into bg: a status tile that's on (D-140)

    // Shape
    readonly property int radius: 10
    readonly property int radiusSmall: 8
    readonly property int spacing: 16
    readonly property int pagePadding: 20
    readonly property int headerHeight: 64
    readonly property int sidebarWidth: 200
    readonly property int navItemHeight: 40
    readonly property int stageDotSize: 8      // Flight progress: the marker before each stage
    readonly property int stageTimeWidth: 104  // Flight progress: "22:51Z – 22:54Z"

    // Spacing scale: every gap, margin and padding below Theme.spacing (16) comes from here
    readonly property int gap2: 2
    readonly property int gap4: 4
    readonly property int gap6: 6
    readonly property int gap8: 8
    readonly property int gap10: 10
    readonly property int gap12: 12
    readonly property int gap14: 14

    // Lines
    readonly property int borderWidth: 1
    readonly property int hairline: 1          // separators
    readonly property int accentBarWidth: 3    // the selected nav item's and fleet row's bar
    readonly property int routeLineWidth: 3    // the flight's track on the map

    // Controls
    readonly property int controlHeight: 36    // buttons and text fields
    readonly property int chipHeight: 32       // New Flight's kind chips, the collapse button
    readonly property int pillHeight: 28       // the connection pill
    readonly property int badgeHeight: 22      // the fleet's status badges
    readonly property int logoSize: 30         // the sidebar's logo
    readonly property int shortFieldWidth: 160 // New Flight's flight number
    readonly property int dialogWidth: 600     // New Flight

    // Dots
    readonly property int feedDotSize: 7       // the event log's markers
    readonly property int feedDotOffset: 5     // lines them up with the first text line
    readonly property int trackDotSize: 14     // the aircraft on the map
    readonly property int trackDotBorder: 3

    // Tiles, tables and columns
    readonly property int tileWidth: 150       // StatTile, StatusTile
    readonly property int tileHeight: 92
    readonly property int tableHeaderHeight: 36
    readonly property int tableRowHeight: 44
    readonly property int ledgerRowHeight: 40
    readonly property int ledgerDateWidth: 110
    readonly property int cashTileWidth: 360
    readonly property int sideColumnWidth: 340 // Live's Flight progress, the Flights list, the summary's money (D-139)
    readonly property int liveFeedWidth: 300   // Live's event log; the planned flight's side column
    readonly property int summaryHeight: 520
    readonly property int summaryMapMinHeight: 240
    readonly property int checkMarkWidth: 18     // the debrief's ✓ / ✗ column

    // The dashboard header (docs/dashboard.md, D-151, D-152, D-182, D-184; mockup docs/images/dashboard-header-v2.html)
    readonly property int headerPadding: 20
    readonly property int fontSizeHeaderIcao: 24        // "XLY 8": just under the airline name
    readonly property int fontSizeHeaderAirline: 26     // "Soxxly Airlines"
    readonly property int headerAirlineHeight: 34       // its line: "XLY 8" and the clock centre on it
    readonly property int fontSizeAirportCode: 36       // "KSAN"
    readonly property int fontSizeHeaderClock: 18       // the block clock pill
    readonly property int fontSizeStatus: 19            // the current status line
    readonly property int fontSizeAirportIcon: 26       // the departure and arrival glyphs
    readonly property int headerClockPillHeight: 30
    readonly property int headerTopHeight: 184          // the identity line, the arc and the airports
    readonly property int airportBlockTop: 62           // the airplane glyph's top
    readonly property int routeMidTop: 93               // "Flight 8", the cities and the distance, below the arc
    readonly property int arcRise: 28                   // the arc's peak above its ends: kept flat
    readonly property int arcGlyphGap: 6                // between an airplane glyph and the marker at the arc's end
    readonly property int arcWidth: 5                   // the flown part of the arc
    readonly property int arcDotSize: 3                 // the dashed part still to fly
    readonly property color arcTodTick: "#E2E8F0"       // the plan's top of descent across the arc (D-192)
    readonly property int arcTodTickWidth: 2
    readonly property int arcTodTickLength: 14
    readonly property int arcMarkerSize: 20
    readonly property int arcMarkerBorder: 3
    readonly property int airportBlockWidth: 240
    readonly property int phasePillHeight: 30
    readonly property int phasePillGap: 2               // between sections
    readonly property color phaseDone: "#06B6D4"        // a section flown (cyan); the current one is `good`
    readonly property color textOnGood: "#052E16"
    readonly property color textOnDone: "#083344"
    // The pill's marker: a red square with a pointed top, raised (a gradient, a white edge, a shine and a shadow),
    // its point in the pill; the sub-phase under it.
    readonly property int pillMarkerWidth: 14
    readonly property int pillMarkerHeight: 15
    readonly property int pillMarkerOverlap: 6
    readonly property int pillMarkerRowHeight: 32       // the marker and its label
    readonly property color pillMarkerTop: "#FCA5A5"
    readonly property color pillMarkerBottom: "#B91C1C"
    readonly property color pillMarkerEdge: "#FFFFFF"
    readonly property color pillMarkerShadow: "#B3000000"
    readonly property real pillMarkerShadowOffset: 1.5
    readonly property real pillMarkerShadowBlur: 3
    readonly property real pillMarkerShoulder: 0.36     // where the point meets the sides, as a share of the height
    readonly property real pillMarkerShineOpacity: 0.55
    // The status lines: three slots, newest on top; a new line rolls in and the others drop, smaller and greyer.
    readonly property int statusBlockHeight: 76
    readonly property int statusSlot1Y: 32               // the line before (the newest is at 0)
    readonly property int statusSlot2Y: 53               // the one before that
    readonly property int statusLeaveY: 70               // where the oldest fades out
    readonly property int fontSizeStatusLeaving: 11      // the newest is fontSizeStatus, then fontSizeFeed, fontSizeSmall
    readonly property int statusEnterY: -12
    readonly property int statusCollapsedHeight: 26     // the collapsed header's one line (D-187)
    readonly property int fontSizeCollapsedCode: 22     // "KSAN" beside the collapsed pill
    readonly property int statusEnterAngle: 80          // rolled back about its top edge as it comes in
    // Animations: short and only on a change (no continuous animation, CLAUDE.md).
    readonly property int statusRollMs: 600
    readonly property int statusFadeMs: 450
    readonly property int statusRemoveMs: 700           // a line that faded out is dropped after the roll
    readonly property int pillMarkerMoveMs: 450
    readonly property int pillColorMs: 400
    readonly property real bounceOvershoot: 1.4
    // The flown arc's colours by altitude, fading smoothly (not in bands): ground, a third, two thirds, cruise
    readonly property color arcGround: "#EF4444"
    readonly property color arcLow: "#EAB308"
    readonly property color arcHigh: "#22C55E"
    readonly property color arcCruise: "#3B82F6"
    readonly property int checkPointsWidth: 36   // the debrief's "−6" column

    // The instrument panel below the header (D-179; mockup docs/images/dashboard-preflight-v1.html)
    readonly property color panelInsetTop: "#0A0D11"
    readonly property color panelInsetBottom: "#0C0F14"
    readonly property color panelInsetShade: "#D9000000"  // the inner shadow along the top edge
    readonly property color panelEdgeLight: "#0DFFFFFF"   // the faint light along the bottom edge
    readonly property int panelInsetShadeHeight: 10
    readonly property int panelRadius: 12
    readonly property int panelPaddingTop: 24
    readonly property int panelPaddingSide: 30
    readonly property int panelColumnGap: 22
    readonly property int screwSize: 11
    readonly property int screwInset: 9
    readonly property color screwLight: "#5A626D"
    readonly property color screwDark: "#23282F"
    readonly property color screwSlot: "#111418"
    readonly property color panelTitleBg: "#15191F"       // a group's engraved placard
    readonly property color panelTitleText: "#C9CED6"
    readonly property int fontSizePanelTitle: 11
    readonly property real panelTitleSpacing: 1.8
    // Radio-stack readouts: amber digits glowing in a dark window
    readonly property color readoutTop: "#1B2027"
    readonly property color readoutBottom: "#14181E"
    readonly property color readoutWindow: "#070605"
    readonly property color readoutPlacard: "#9AA3AF"
    readonly property color amber: "#FFB23E"
    readonly property color amberDim: "#B8762A"           // tags and the holds line
    readonly property color amberGlow: "#8CFFA028"
    readonly property color amberBlank: "#3A2A12"         // the dash for a value not known yet
    readonly property color readoutRed: "#FF5A45"         // the clearance's last 5 minutes
    readonly property color readoutRedGlow: "#99FF4632"
    readonly property color readoutFlagText: "#1A1205"    // text on an amber "changed" tag
    readonly property int readoutRadius: 6
    readonly property int readoutWindowRadius: 3
    readonly property int readoutWindowHeight: 30  // mockup v2 (D-203)
    readonly property int readoutGap: 10
    readonly property int fontSizeReadoutTitle: 10
    readonly property real readoutTitleSpacing: 1.4
    readonly property int fontSizeReadout: 22
    readonly property int fontSizeReadoutText: 17         // words: SID, initial altitude, taxi route
    readonly property int fontSizeReadoutUnit: 13
    readonly property int fontSizeReadoutTag: 11
    readonly property int squawkDigitWidth: 30
    readonly property int squawkDigitHeight: 36
    readonly property int fontSizeSquawk: 24
    readonly property color readoutLatestText: "#1A1205"  // LATEST: the placard lit amber (D-203)

    // The in-flight panel (D-194..D-198, D-203; mockup docs/images/dashboard-inflight-v2.html)
    readonly property int inflightPaddingTop: 12
    readonly property int inflightPaddingSide: 28
    readonly property int inflightRowGap: 6
    readonly property color lampStripRule: "#0AFFFFFF"
    readonly property int lampWidth: 74
    readonly property int lampSize: 30          // the bezel; the lens is 4 px inside it
    readonly property int lampGap: 26
    readonly property int lampGlow: 16
    readonly property color lampBezelLight: "#9AA2AC"
    readonly property color lampBezelDark: "#2C3138"
    readonly property color lampPlacardText: "#C9CED6"
    readonly property int fontSizeLampPlacard: 10
    readonly property real lampPlacardSpacing: 1.2
    // Each lens's colours (D-179): dark tinted glass when off, an incandescent bulb behind it when lit.
    readonly property var lampLenses: ({
        "red":   { off: ["#3B1210", "#1A0807"], on: ["#FFF1D6", "#FF9A72", "#E3301F", "#7D0F09"], glow: "#73FF4028" },
        "white": { off: ["#2C2D2B", "#141513"], on: ["#FFFFFF", "#FFF4DC", "#E9D9B6", "#8D7E62"], glow: "#59FFECC8" },
        "amber": { off: ["#3A2808", "#191104"], on: ["#FFF6D0", "#FFD060", "#E89A10", "#7A4A02"], glow: "#73FFAA1E" },
        "green": { off: ["#10301A", "#07160B"], on: ["#EAFFE0", "#8CF59A", "#22B44A", "#0B4A1C"], glow: "#613CDC64" }
    })
    readonly property int lampFlashMs: 1000     // 1 s lit, 1 s dark (D-195); the model's timer
    // Steam gauges, drawn on a 220-unit face and scaled to gaugeSize
    readonly property int gaugeSize: 200         // 0.15.1: 128 was hard to read in flight (D-205)
    readonly property color gaugeBezelLight: "#7D8590"
    readonly property color gaugeBezelDark: "#2A2F36"
    readonly property color gaugeFace: "#0B0C0D"
    readonly property color gaugeTick: "#E8E8E8"
    readonly property color gaugeNumeral: "#F0F0F0"
    readonly property color gaugeCaption: "#9AA3AF"
    readonly property color gaugeRedArc: "#C43A2C"
    readonly property color gaugeBug: "#E0B030"
    readonly property color gaugeNeedle: "#F4F4F4"
    readonly property color gaugeHub: "#22262B"
    readonly property color gaugeHubRing: "#555C66"
    readonly property color gaugeWindow: "#000000"
    readonly property color gaugeWindowRing: "#3B4048"
    readonly property color gaugeGlare: "#29FFFFFF"
    readonly property color gaugeLabel: "#9AA3AF"
    readonly property int fontSizeGaugeLabel: 10
    // A needle glides to each new value over the samples' one second, so it moves steadily a second behind (D-205)
    readonly property int needleGlideMs: 1000
    // Flips (D-198): a half-turn about the horizontal axis, in a wave left to right
    readonly property int flipDurationMs: 500
    readonly property int flipWaveStepMs: 60
    // The ATC card's columns: ALTITUDE and APPROACH wide, SPEED narrow (D-203)
    readonly property var atcColumnWeights: [1.3, 1, 0.6, 1.3]
    readonly property int readoutColumns: 4

    // The Fleet screen (D-241): the airline's panel, the airplane cards and the slots.
    readonly property color glyphReady: good          // a plane Dave can fly now
    readonly property color glyphDelivery: warn       // awaiting its factory delivery
    readonly property color glyphCurrent: accent      // the plane with the planned or in-progress flight
    readonly property color glyphAir: "#4B5563"       // flying a background leg (rotations, later)
    readonly property color rotation: "#2DD4BF"       // a rotation plane's border and ribbon (later)
    readonly property color journey: "#A78BFA"        // a journey plane's border and ribbon (later)
    readonly property color slotLockedBg: "#12161C"
    readonly property color slotLockedBorder: "#1D232B"
    readonly property color slotEmptyBorder: "#3A4656"
    readonly property color slotNextBorder: "#4A3A17"
    readonly property color cardBorder: "#33404F"
    readonly property int fleetPanelWidth: 280
    readonly property int fleetLogoSize: 56
    readonly property int fleetCardWidth: 270
    readonly property int fleetCardHeight: 192
    readonly property int fleetGlyphSize: 52
    readonly property int lockedTileWidth: 72
    readonly property int lockedTileHeight: 50
    readonly property int lockedTilesShown: 7          // then "+N more"
    readonly property int fontSizeFleetLevel: 30       // "4 / 20"
    readonly property int fontSizeFleetTitle: 16       // a card's "737-800 Two-class"
    readonly property int fontSizePlus: 30             // an empty slot's "+"
    readonly property int progressBarHeight: 8
    readonly property int dashLength: 4                // an empty slot's dashed border, in stroke widths
    readonly property int emptySlotBorderWidth: 2
    readonly property int dropAreaHeight: 260          // where a SimBrief screenshot goes in
    readonly property int formLabelWidth: 130          // Add Aircraft's field labels
    readonly property int formUnitWidth: 18            // "lb", "ft" after a field
    readonly property int holdBarHeight: 26            // the hold split, drawn to scale
    readonly property int portraitHeight: 150          // the aircraft screen's picture of the plane
    readonly property int portraitGlyphSize: 110

    // The dispatch report (docs/dispatch.md, D-234; board Final on the design canvas)
    // The ring: each family's shades in the order its lines come (fuel red; airport orange; crew purple; catering and
    // ATC blue; profit green).
    readonly property var ringFuel: ["#EF4444", "#F87171"]
    readonly property var ringAirport: ["#F97316", "#FDBA74", "#FB923C", "#EA580C", "#FED7AA"]
    readonly property var ringCrew: ["#A855F7", "#C084FC", "#7C3AED"]
    readonly property var ringService: ["#38BDF8", "#0EA5E9", "#7DD3FC"]
    readonly property var ringProfit: ["#22C55E"]
    readonly property int ringSize: 280
    readonly property int ringRadius: 118          // the stroke's middle
    readonly property int ringStroke: 28
    readonly property int ringStrokeLifted: 34
    readonly property int ringLift: 10             // how far a hovered or selected slice moves out
    readonly property int ringLiftMs: 150          // the lift's transition: a short Behavior, not a running animation
    readonly property real ringGapDegrees: 0.8     // between slices
    readonly property int ringTagRadius: 168       // the hover tag's centre, just outside the ring
    readonly property color incomeText: "#4ADE80"
    readonly property color costText: "#F87171"
    readonly property color noteText: "#FBBF24"    // what changed since the last import
    readonly property int fontSizeRingValue: 36
    readonly property int fontSizeReportTitle: 28
    // The split-flap board: 20 tiles a row
    readonly property color boardBg: "#0B0C0E"
    readonly property color tileTop: "#1D1F23"
    readonly property color tileBottom: "#17191C"
    readonly property color tileSeam: "#050607"
    readonly property color tileLabel: "#F2EEE3"   // the labels, lit
    readonly property color tileWaiting: "#8B95A3" // the labels before import
    readonly property color tileCost: "#FFC857"    // costs settle in amber
    readonly property color tileGood: "#7CF0A6"    // fares and profit settle in green
    readonly property color tileLoss: "#FF7A7A"
    readonly property color boardRowHover: "#3A424D"
    readonly property int boardTileWidth: 22
    readonly property int boardTileHeight: 30
    readonly property int boardTileGap: 2
    readonly property int boardRowGap: 3
    readonly property int fontSizeBoardTile: 19
    readonly property string boardFontFamily: "Consolas"
    readonly property int flapTickMs: 35           // one character step of the flip; it ends after the row settles
    readonly property int flapRowDelay: 7          // ticks between rows
    readonly property int flapColumnDelay: 2       // ticks between tiles
    // The working column, the load and the plan
    readonly property int workingWidth: 300
    readonly property int acceptHeight: 56
    readonly property int fontSizeAccept: 18
    readonly property int seatStripHeight: 18
    readonly property int weightTapeHeight: 8
    readonly property int weightLabelWidth: 150
    readonly property int weightValueWidth: 170
    readonly property int fontSizePlanAirport: 30
    readonly property int fontSizePlanCell: 22
    readonly property int planLineHeight: 3
    readonly property int reportBadgeHeight: 28
    readonly property int formWidth: 720

    // Window
    readonly property int windowWidth: 1300
    readonly property int windowHeight: 760
    readonly property int windowMinWidth: 1100
    readonly property int windowMinHeight: 600

    // Type
    readonly property string fontFamily: "Segoe UI Variable"
    readonly property int fontSizeLabel: 11  // small caps labels above values
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeFeed: 13   // the event log
    readonly property int fontSizeBody: 14
    readonly property int fontSizeTitle: 18
    readonly property int fontSizeValue: 28
    readonly property int fontSizeClock: 44
    readonly property real labelLetterSpacing: 1.2
    readonly property string monoFontFamily: "Consolas"  // the route box

    // Icons: Windows 11 system icon font, nothing to bundle (D-062)
    readonly property string iconFontFamily: "Segoe Fluent Icons"
    readonly property int fontSizeIcon: 16
}
