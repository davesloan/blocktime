# Claude's debrief review: real calls

What the first real calls to the Messages API showed (D-156, D-161, `docs/debrief.md` section 5). Both were made with
`blocktime-debrief --claude` on the KLAS → KSAN fixture (`tests/data/flight-klas-ksan/`), 2026-11-24, model
`claude-opus-5-5`, effort `medium`.

## Size, time and cost

| Call | Input tokens | Output tokens (thinking) | Time | Cost at $4 / $20 per million |
|---|---|---|---|---|
| 1 | 5,802 | 1,520 (685) | 19 s | about $0.05 |
| 2 (timeline with heights and speed spells) | 5,963 | 1,859 (1,056) | about 20 s | about $0.06 |

- The brainstorm's estimate was $0.12–0.16 a flight. The real figure is about a third of that: the condensed timeline
  is about 6,000 tokens, not the 8,000–15,000 assumed, and thinking at `medium` stays around 1,000 tokens.
- Dave's Max plan includes $100 of API credit a month (2026-11-24), so cost isn't a constraint.
- The request, the headers (`anthropic-version: 2026-10-21`, the fallback beta) and the structured JSON answer worked
  first time; the answer parsed with `parseReviewResponse`.

## What the first review got wrong, and why

The review was good on the landing (state at 1,000 ft → autopilot off → sink building → firm touchdown) and on ATC.
It made two factual slips, both from facts the timeline didn't give it:

- **"Watch speed in the climb"** for the 250 kt check, whose 17 s spell was in the descent. The check's detail ("17 s
  above 260 kt") had no time or direction.
- **"The autopilot came off at about 800 ft"**: the timeline gave the time only, so it estimated a height (it was 350 ft
  above the ground).

Fixed by giving the timeline the facts (test `DebriefReview.TimelineGivesHeightsAndSpeedSpells`):

- every cockpit change in the air says how high it happened ("autopilot off at 350 ft above the ground");
- each spell above 260 kt below 10,000 ft gets its own line ("above 260 kt below 10,000 ft for 17 s, descending
  through 9,972 ft, fastest 275 kt").

The second call used both correctly ("On descent, slow to 250 kt before reaching 10,000 ft"; "You kept the autopilot to
350 ft").

**The rule that follows:** when a review states a number the input doesn't have, add that number to the timeline;
don't argue with the prompt.

## Dave's notes on the first in-app review (milestone 10 sim test, 2026-11-25)

The BBJ2 leg KSAN → KSJC. Dave disagreed with the review (form step 9, failed):

- **"Unstable at 1,000 ft"**: BlockTime's gear bug (`docs/notes/msfs-observed-behaviour.md`) stored the gear as up, so
  the stable check was missed and Claude explained the miss. The timeline also gave the raw "flaps handle 8", which
  Claude read as short of flaps 40 (detent 8 is flaps 40). Both fixed in BlockTime: the gear threshold, and the
  timeline now says "flaps 40".
- **The 3,300 ft level-off** was the FMC (VNAV) holding the 3,200 ft constraint at the next waypoint; the aircraft
  doesn't know it's been cleared lower. Claude called it a fault and put it in "work on", against D-157 (ATC is
  information only).
- **"Point seven"**: controllers say ".7" for 121.75 and expect ".75" read back; Claude called it an incomplete
  frequency.
- **Conflicting landing advice**: it recommended holding VREF to the flare, then criticised touching down 10 kt below
  VREF, on a 100 fpm, 1.11 G landing.

The instructions now say (D-171, guarded by `DebriefReview.InstructionsCarryDavesCorrections`): the checks are the
verdict (never call a passed check a fault); ATC compliance never in summary or "work on"; a level-off on the approach
is often a published constraint the FMC is flying; no remarks on frequencies or phraseology; up to about 10 kt below
VREF at touchdown is normal flare bleed-off; advice only for missed checks or the landing's facts, no technique
preferences; keep the advice consistent.

**Call 3** (leg 1 replayed with the gear marked down from 19:50Z, a scratch copy): 5,831 tokens in, 1,190 out (515
thinking), about $0.05. Score 100, ★5, nothing to work on; the level-off was "most likely the FMC holding a published
altitude constraint", touchdown speed "a normal bleed-off through the flare", no frequency remark.

## Still to watch

- It called the tower's "RNAV FLAAR" (the RNAV departure to its first fix) an oddity next to the RADYR2 clearance.
  That's a harmless misread of US RNAV phraseology; leave it unless it recurs.
- The replay tool reads no database, so its reviews say "first logged flight". The app passes the last five flights'
  misses.

## The KBFI → KPDX delivery (2026-11-28)

The destination altimeter check read both altimeters once, 60 s after the transition level. Dave set 1021.7 hPa
(30.17, ATIS November) 84 s after it, so the check stored "8.5 hPa off" (STD against the QNH). The timeline gave
Claude that result and, separately, the correct setting a little later. With no time for the check's reading, it
invented a cause: "cross-check the reported QNH against the simulator's own weather". The rule above, applied: the
check now measures what happened, the seconds until the setting matched (D-254), so the line Claude gets reads
"missed (1 min 24 s after the transition level)".
