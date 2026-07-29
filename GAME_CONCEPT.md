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

## Platform

**Mobile (iOS + Android)**, not Steam. Idle games with short daily
touchpoints, offline progress, and rewarded-ad speedups fit mobile
play patterns much better than a PC storefront — that's the actual
reason for the switch, not just monetization preference.

Mobile has no single unified leaderboard service the way Steam does
(Game Center and Google Play Games Services don't share data with
each other), so the competitive layer moves to a lightweight cloud
backend (Firebase) that both platforms write to — see "Competitive
Layer" below.

## Competitive Layer

Cloud leaderboard (Firebase-backed, one shared table across iOS +
Android — see "Tech Stack"):

- **Season ladder (resets weekly, matching the demand-shift cycle):**
  ranks by Empire Value gained *this season*.
- **All-time hall of fame:** lifetime peak Empire Value / total sales.

Anti-pay-to-win guardrail, adapted for mobile: nothing purchasable —
ad-watch or Gems — ever increases Cash, Genetics tier, or a sale's
price/quality directly. Every speedup is **time compression only**: it
lets you reach the same outcome sooner, not a better outcome. Honest
caveat, not papered over: because seasons are a fixed real-time
window, compressing time still lets a spender or heavy ad-watcher fit
more grow/sell cycles into that window than a patient F2P player —
that's an inherent tension in any idle game with paid speedups plus a
timed leaderboard, not something unique to this design, and not fully
eliminable while keeping speedups meaningful. Ad-watching is free and
equally available to every player regardless of spend, which keeps the
paid path from being the *only* way to compress time.

## Monetization

- **Rewarded ads (opt-in only, never forced):** watching a video grants
  Gems, spendable on instant-grow speedups. Capped per day so it stays
  "occasional boost," not "watch ads constantly."
- **Gems (buyable currency):** same Gems ads grant, purchasable via IAP
  for players who'd rather pay than watch. Spent on instant-grow
  speedups and cosmetics — never on anything that raises the
  Cash/Genetics/price ceiling.
- Cosmetic packaging/branding, grow-room decor and themes
- No loot boxes, no gacha, no player marketplace, no real-money
  currency that touches another player's account

## Tech Stack

- **Unity** (2022 LTS or newer), 2D — grow-room/plot views, simple UI-
  driven mixing screen; Android + iOS build targets
- **Firebase** (or equivalent) for the shared cross-platform
  leaderboard and achievements — chosen specifically because Game
  Center/Google Play Games Services don't share data with each other
- **Unity LevelPlay/Ads** for rewarded video ads, **Unity IAP** for
  Gems purchases — both third-party/official packages, integrated
  behind an interface with a safe local fallback so the project
  builds without them present (same pattern used for the earlier Steam
  integration)
- Idle/offline-time math and the seasonal demand table are pure C#, no
  server needed for the core loop — fully offline-capable except
  leaderboard submission and ad/IAP calls
- Save format: local JSON, cloud-sync-compatible

## MVP Scope (first playable milestone)

1. One grow room, 3 starter strains, manual mix + sell
2. Cash → buy more plots / faster growth
3. One breeding pair mechanic → produces 3 seeds with blended genetics
   (proves the long-horizon hook)
4. A single seasonal demand table that rotates once, so the "read the
   market" loop is provable
5. Gems + instant-grow speedup, with a local-fallback ad/IAP service so
   the loop is testable before real ad/IAP SDKs are wired in
6. Cloud leaderboard integration: submit Empire Value at season end,
   show top 10 + player's rank
7. Offline growth calculation on relaunch

Everything past this (full ingredient tree, cosmetics, achievements,
art pass, more seasons) layers on top once the loop is proven fun.

## Open Questions / Next Steps

- **Content-policy risk (mobile-specific, real):** Apple and Google
  are both stricter than Steam about drug-themed content — neither has
  Steam's precedent (*Schedule I*, *Weedcraft Inc*). This needs an
  actual read of the current App Store Review Guidelines and Google
  Play Developer Policy before investing in a full art/marketing pass,
  and may call for softening real-world framing further (fictional
  substance names, avoid real-drug branding) specifically for the
  mobile listing. See `docs/mobile-publishing-checklist.md`.
- Pick a visual style (pixel art vs. flat vector/isometric) — affects
  art scope for grow rooms
- Design the actual ingredient/effect-tag table and season-to-season
  demand rotation rules
- Scaffold the actual Unity project structure and ad/IAP/leaderboard
  integration once the loop above is confirmed
