# Strain Empire — Systems Design (v1)

Detailed mechanics for the loop described in `GAME_CONCEPT.md`. Numbers
here are a first-pass baseline for the MVP build — expect tuning once
it's playable (tracked as a later pass, not blocking implementation).

## Genetics & Breeding

Every strain has four growth traits, 0–100:

| Trait | Effect |
|---|---|
| Potency | Feeds into the Potency effect tag and base value |
| Yield | Raw product quantity per harvest |
| Speed | Reduces grow time |
| Resilience | Reduces bad-harvest event chance |

...and a baseline **Effect Profile** (Relaxation / Energy / Focus,
0–100 each, Potency covered by the trait above) — the strain's
"character" before any mixing. Both the traits and the baseline
profile are inherited on breeding, so genetics is a single long-term
investment that shapes what a strain is good at.

**Starter strains** (Tier 0): traits and baseline profile rolled
20–35.

**Breeding**: pick two mature strains as parents. Offspring traits and
baseline profile = average of the two parents, then each value rolls a
10% chance of a ±(5–15) mutation, clamped to 0–100. Averaging biases
offspring toward parent quality (so breeding good strains together is
the correct strategy), while the mutation roll keeps outcomes non-
deterministic enough to matter.

**Genetics Tier** = `floor(average of 4 traits / 20)`, 0–4. Tier
gates: which ingredients are unlocked, and a base value multiplier of
`1 + 0.25 × tier`.

**Grow time** (hours) = `4 + (average traits / 100) × 20` → 4h for a
weak Tier 0 strain up to 24h for a maxed Tier 4 strain. Better
strains take longer to grow — a deliberate plot-allocation trade-off.

**Breeding cooldown**: fixed 12h after both parents are mature before
a seed batch (3 seeds) is produced. The 3 seeds are independently
rolled (each gets its own mutation check), so the player picks the
best of the batch to carry forward — that selection is what gives
genetics real upward progress across generations. A single averaged
result with no selection would just random-walk around the parents'
tier rather than climbing.

**Pacing target**: reaching Tier 4 from Tier 0 stock takes roughly
4–6 breeding generations. Each generation = grow parents to maturity
(~12–20h) + 12h breeding cooldown + grow the offspring to confirm its
rolled stats (~16–24h) ≈ 40–56h. Five generations ≈ 200–280 hours,
i.e. **roughly 8–12 days** of idle/offline time for a top strain —
matches the original "days or weeks" pacing goal without requiring
constant active play.

## Ingredients & Mixing

A harvested batch is processed with up to **3 ingredient slots**
before sale. Each ingredient shifts the batch's Effect Profile
(Potency/Relaxation/Energy/Focus) and/or a separate **Quality** score
(0–100, craftsmanship independent of season).

`FinalTag = clamp(strain baseline tag + Σ ingredient deltas, 0, 100)`

| Ingredient | Effect | Quality | Cost | Unlock |
|---|---|---|---|---|
| Sunroot Extract | +15 Energy, −5 Relaxation | — | 8 | Tier 0 |
| Mossveil Leaf | +15 Relaxation, −5 Energy | — | 8 | Tier 0 |
| Clarid Crystal | +12 Focus, −8 Potency | — | 10 | Tier 0 |
| Emberash Resin | +18 Potency, −6 Focus | — | 14 | Tier 0 |
| Silverdew Drops | — | +8 | 12 | Tier 0 |
| Thistlebind Fiber | +10% batch yield | −4 | 6 | Tier 0 |
| Glowcap Spores | +10 Potency, +10 Energy, −10 Relaxation | — | 16 | Tier 1 |
| Duskmint | +10 Relaxation, +10 Focus, −8 Energy | — | 15 | Tier 1 |
| Ironbark Ash | +6 Potency | +6 | 18 | Tier 2 |
| Prism Dew | +12 to all four tags | −15 | 20 | Tier 2 |

The 3-slot cap keeps combos a real puzzle (10 ingredients, choose 3)
rather than "use everything." Picking the right 3 for the current
season's demand is the week-to-week skill expression, sitting on top
of the slower genetics game.

## Seasonal Market

Season length: **7 real days**, matching the weekly leaderboard reset.
Each season rolls one demand archetype (multipliers on the 4 effect
tags):

| Archetype | Multipliers |
|---|---|
| Heavy Hitter Week | Potency ×1.8, others ×0.8 |
| Chill Wave | Relaxation ×1.8, others ×0.8 |
| Rise & Grind | Energy ×1.8, others ×0.8 |
| Clarity Season | Focus ×1.8, others ×0.8 |
| Balanced Market | all ×1.1 |
| Volatile Swing | two random tags ×1.5, other two ×0.6 |

**Next-season preview**: revealed 24h before rotation, so players can
pre-mix a batch in advance. No full-season foresight — this is a
prep window, not a solved game.

**Sell price**:

```
BasePrice        = 5 (per unit, baseline)
TierMultiplier    = 1 + 0.25 × GeneticsTier
QualityMultiplier = 0.5 + Quality / 100
DemandScore       = avg over 4 tags of (tagValue / 100 × seasonMultiplier[tag])

UnitPrice = BasePrice × TierMultiplier × QualityMultiplier × DemandScore
```

No randomness in the sale itself — price is fully determined by
genetics + mixing + how well that matches the current season. Skill,
not luck, drives the score.

## Economy Balance (first pass)

- Starting cash: 200; 1 free grow plot to start
- Additional plot cost: `150 × 1.6^(n-1)` for the nth added plot
- Offline accrual cap: 8h base, extendable to 24h via the QoL MTX pass
  (rate unchanged — see `GAME_CONCEPT.md` monetization section)

## Leaderboard Score

```
Empire Value = Cash on hand
             + Σ (grow plot value)
             + Σ (strain genetics tier × 500)
```

Submitted as the Steam Leaderboard score at season end (season ladder)
and tracked as a running peak (all-time hall of fame), per
`GAME_CONCEPT.md`.

## Tuning Notes

These numbers are a deliberately concrete starting point so the MVP
(task: core grow/mix/sell loop) has real data to build against, not a
placeholder. Expect a dedicated balance pass once the loop is
playable — growth-time curve, ingredient costs, and demand swing
magnitude are the most likely to need adjustment.
