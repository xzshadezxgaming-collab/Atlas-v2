# Deep Dig — Game Concept

## Pitch

An idle mining game: you run an automated excavation rig that digs
deeper and deeper into a procedurally-stratified world, generating ore
and gold along the way. Sell resources, buy automation upgrades, hit a
depth wall, then **collapse the rig** (prestige) to descend into a new,
harder layer with permanent bonuses. Depth is the score that matters —
it's visual, it's legible at a glance, and it maps directly onto a
weekly competitive ladder.

Why this theme over a generic "clicker": digging has a built-in sense
of *place* (a depth number, a visual shaft filling the screen) that
pure number-go-up idles lack, while still being trivial to represent
in 2D layered sprites — cheap to build, easy to reskin for cosmetics.

## Core Loop

1. **Drills** auto-mine layers of rock, producing **Ore**.
2. Ore auto-sells (or is manually sold) for **Gold**.
3. Gold buys:
   - More/faster drills (horizontal scaling)
   - Depth-piercing upgrades to break through harder strata (vertical progression)
   - Capacity upgrades (idle earnings cap raised, so offline time is worth more)
4. Deeper layers have better ore but tougher rock (need upgrades to
   progress) — this is the pacing lever.
5. When progress stalls, the player **collapses the rig** (prestige):
   depth resets to 0, but they earn **Core Shards** based on max depth
   reached, spent on permanent multipliers (drill speed, sell price,
   offline cap, starting depth on next run).

This is the standard "idle + prestige" shape (Cookie Clicker /
AdVenture Capitalist lineage) but themed so prestige feels diegetic —
you're literally caving in the shaft to start a fresh, deeper dig — and
so leaderboard progress (depth) is the same number driving moment-to-
moment feedback, not a separate abstract score.

## Resources & Progression

| Resource | Role |
|---|---|
| Ore | Raw output of drills, per-layer value scaling |
| Gold | Ore sold; spent on in-run upgrades |
| Core Shards | Prestige currency; spent on permanent meta-upgrades |
| Depth (m) | The progression axis and the competitive score |

Meta-upgrade tree (bought with Core Shards, persists across resets):
drill speed, sell multiplier, offline earnings cap, starting depth
boost, automation unlocks (auto-sell, auto-collapse-at-depth-X).

## Competitive Layer

Two leaderboards, both via **Steam Leaderboards** (Steamworks.NET —
native, free, no custom backend needed):

- **Season ladder (resets weekly):** ranks by max depth reached *this
  week*. Because everyone's in-season progress starts even relative to
  their permanent meta-upgrades, this rewards active play and skillful
  upgrade-path decisions each week, not just lifetime playtime or
  wallet size. This is the "competitive" hook.
- **All-time hall of fame:** lifetime max depth / total gold earned —
  a slower-moving bragging-rights board for long-term players.

Anti-pay-to-win guardrail: nothing purchasable directly increases
Depth or Core Shards. MTX only ever buys convenience or cosmetics (see
below), so a top-of-leaderboard run can't be bought outright — only
sped up modestly via QoL.

## Monetization (minimal, Steam-review-safe)

- Cosmetic drill/rig skins and shaft backgrounds (one-time purchases,
  purely visual)
- "Extended offline cap" QoL pass — raises the ceiling on offline
  earnings, doesn't change earn rate, so it saves time rather than
  buying rank
- Small supporter pack — cosmetic flair + name in in-game credits
- No ads, no loot boxes, no energy/stamina gates

## Tech Stack

- **Unity** (2022 LTS or newer), 2D pipeline — layered sprite shaft,
  simple parallax, minimal 3D need
- **Steamworks.NET** for: Steam Leaderboards (season + all-time),
  Steam Cloud (save sync), Steam Achievements (depth milestones,
  prestige counts), Steam Stats
- Idle math (offline-time-elapsed calculations) is pure C#, no server
  needed — the whole game can run fully offline except leaderboard
  submission
- Save format: local JSON + Steam Cloud sync; no external DB required

## MVP Scope (first playable milestone)

1. Single shaft, 3 rock tiers, 1 drill type, manual sell
2. Gold → buy more drills / drill speed
3. One prestige loop (collapse → Core Shards → 3 meta-upgrades)
4. Steam Leaderboard integration: submit max depth on collapse, show
   top 10 + player's rank
5. Offline earnings calculation on relaunch

Everything past this (cosmetics, achievements, seasonal reset
scheduling, art pass) layers on top once the loop is proven fun.

## Open Questions / Next Steps

- Pick a visual style (pixel art vs. flat vector) — affects art scope
- Decide season length (weekly vs. bi-weekly) and reset mechanics
  (hard reset vs. rolling window)
- Scaffold the actual Unity project structure and Steamworks.NET
  integration once the loop above is confirmed
