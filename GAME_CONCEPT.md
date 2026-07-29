# Strain Empire — Game Concept

## Pitch

An idle cultivation/empire game: you grow and breed strains, mix
ingredients into finished products, and sell into an NPC market whose
demand shifts every season. Growing a strong strain from scratch takes
real time — days, sometimes weeks of idle/offline progress — so the
game rewards players who breed smart and time the market well, not
just whoever plays the most hours. Commercial viability for the theme
is well established (*Schedule I*, *Weedcraft Inc*, *Drug Dealer
Simulator* all shipped fine on Steam).

No player-to-player trading or economy: all buying/selling is against
an NPC market. This keeps the whole project a normal idle game — no
real-money marketplace, no trade infrastructure, no RMT/fraud surface,
no loot-box-adjacent regulatory questions. The competitive angle comes
purely from how well you run your own operation, not from market
manipulation between players.

## Core Loop

1. **Grow** strains in plots/rooms — growth is time-based and
   continues offline (the "idle" part). Genetics (bred, not bought)
   determine base potency/yield/effect profile.
2. **Mix** harvested product with ingredients/processing steps before
   sale — each combination shifts sell price and effect tags (e.g.
   more relaxing vs. more potent vs. higher yield-per-batch). This is
   the game's puzzle layer: same genetics, different mix, different
   value.
3. **Sell** into the NPC market. Demand for each effect profile shifts
   on a **seasonal cycle** — what sold best last week may be
   mid-tier this week, so part of the skill is reading the season and
   adjusting what you grow/mix for.
4. **Breed** strains together to chase better base genetics —
   this is the long-horizon progression axis (a strong strain can take
   days/weeks of real time to cultivate), separate from the
   week-to-week market-reading game.
5. **Expand** the operation (more grow rooms, faster processing,
   automation) using profits — standard idle horizontal scaling.

Two progression speeds by design: genetics/breeding is a slow, weeks-
long meta-project, while market-reading and mixing decisions play out
week to week. That gives both a long-term goal and a reason to log in
every season.

## Resources & Progression

| Resource | Role |
|---|---|
| Raw Product | Harvested from grow plots, base value from genetics |
| Processed Product | Raw product + ingredients/mix, sold for cash |
| Cash | Sold product; spent on plots, processing upgrades, ingredients |
| Genetics tier | Breeding progress; slow, persistent, drives long-term ceiling |
| Empire Value | Net worth (cash + plots + genetics tier) — the competitive score |

Ingredient/mixing system: each ingredient nudges effect tags
(potency/relaxation/yield/etc.) and price. No randomness in the sale
price itself — mix choice deterministically sets the effect profile,
and the *seasonal demand table* (visible to the player) is what
determines how well that profile sells that week. Skill = matching
your mix to current demand, not luck.

## Competitive Layer

Leaderboards via **Steam Leaderboards** (Steamworks.NET — native,
free, no custom backend):

- **Season ladder (resets weekly, matching the demand-shift cycle):**
  ranks by Empire Value gained *this season*. Because demand resets
  each week, players who read the new season's trends well can climb
  regardless of how long they've played overall.
- **All-time hall of fame:** lifetime peak Empire Value / total sales —
  slower-moving, for long-term operations.

Anti-pay-to-win guardrail: nothing purchasable increases Cash,
Genetics tier, or Empire Value directly. MTX is cosmetic/QoL only (see
below), so climbing the ladder is always a function of breeding and
market-reading skill.

## Monetization (minimal, Steam-review-safe)

- Cosmetic packaging/branding, grow-room decor and themes (one-time
  purchases, purely visual)
- "Extended offline cap" QoL pass — raises the ceiling on offline
  growth/earnings, doesn't change growth rate or market odds
- Small supporter pack — cosmetic flair + name in in-game credits
- No ads, no loot boxes, no player marketplace, no real-money currency
  that touches another player's account in any way

## Tech Stack

- **Unity** (2022 LTS or newer), 2D — grow-room/plot views, simple UI-
  driven mixing screen
- **Steamworks.NET** for: Steam Leaderboards (season + all-time),
  Steam Cloud (save sync), Steam Achievements (genetics milestones,
  season ranks), Steam Stats
- Idle/offline-time math and the seasonal demand table are pure C#, no
  server needed — fully offline-capable except leaderboard submission
- Save format: local JSON + Steam Cloud sync; no external DB required

## MVP Scope (first playable milestone)

1. One grow room, 3 starter strains, manual mix + sell
2. Cash → buy more plots / faster growth
3. One breeding pair mechanic → produces a new strain with blended
   genetics (proves the long-horizon hook)
4. A single seasonal demand table that rotates once, so the "read the
   market" loop is provable
5. Steam Leaderboard integration: submit Empire Value at season end,
   show top 10 + player's rank
6. Offline growth calculation on relaunch

Everything past this (full ingredient tree, cosmetics, achievements,
art pass, more seasons) layers on top once the loop is proven fun.

## Open Questions / Next Steps

- Store-page/content rating considerations: fictional framing,
  appropriate age rating and content warnings, per Steam's existing
  precedent for this theme
- Pick a visual style (pixel art vs. flat vector/isometric) — affects
  art scope for grow rooms
- Design the actual ingredient/effect-tag table and season-to-season
  demand rotation rules
- Scaffold the actual Unity project structure and Steamworks.NET
  integration once the loop above is confirmed
