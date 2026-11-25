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

## Still to watch

- It called the tower's "RNAV FLAAR" (the RNAV departure to its first fix) an oddity next to the RADYR2 clearance.
  That's a harmless misread of US RNAV phraseology; leave it unless it recurs.
- The replay tool reads no database, so its reviews say "first logged flight". The app passes the last five flights'
  misses.
