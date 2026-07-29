using System;
using System.Collections.Generic;
using StrainEmpire.Core.Mixing;
using StrainEmpire.Core.Random;

namespace StrainEmpire.Core.Market
{
    public static class MarketSystem
    {
        public const float BasePrice = 5f;

        public static DemandMultipliers GetDemandMultipliers(SeasonArchetype archetype, IRandomSource rng = null)
        {
            switch (archetype)
            {
                case SeasonArchetype.HeavyHitterWeek:
                    return new DemandMultipliers { Potency = 1.8f, Relaxation = 0.8f, Energy = 0.8f, Focus = 0.8f };
                case SeasonArchetype.ChillWave:
                    return new DemandMultipliers { Potency = 0.8f, Relaxation = 1.8f, Energy = 0.8f, Focus = 0.8f };
                case SeasonArchetype.RiseAndGrind:
                    return new DemandMultipliers { Potency = 0.8f, Relaxation = 0.8f, Energy = 1.8f, Focus = 0.8f };
                case SeasonArchetype.ClaritySeason:
                    return new DemandMultipliers { Potency = 0.8f, Relaxation = 0.8f, Energy = 0.8f, Focus = 1.8f };
                case SeasonArchetype.BalancedMarket:
                    return new DemandMultipliers { Potency = 1.1f, Relaxation = 1.1f, Energy = 1.1f, Focus = 1.1f };
                case SeasonArchetype.VolatileSwing:
                    return RollVolatileSwing(rng ?? new SystemRandomSource());
                default:
                    throw new ArgumentOutOfRangeException(nameof(archetype));
            }
        }

        /// Two of the four tags get x1.5, the other two get x0.6.
        private static DemandMultipliers RollVolatileSwing(IRandomSource rng)
        {
            var tags = new[] { EffectTag.Potency, EffectTag.Relaxation, EffectTag.Energy, EffectTag.Focus };
            var boosted = new HashSet<EffectTag>();
            while (boosted.Count < 2)
                boosted.Add(tags[rng.NextInt(0, tags.Length)]);

            float Mult(EffectTag t) => boosted.Contains(t) ? 1.5f : 0.6f;

            return new DemandMultipliers
            {
                Potency = Mult(EffectTag.Potency),
                Relaxation = Mult(EffectTag.Relaxation),
                Energy = Mult(EffectTag.Energy),
                Focus = Mult(EffectTag.Focus),
            };
        }

        /// docs/systems-design.md "Sell price" formula.
        public static float ComputeUnitPrice(int geneticsTier, EffectProfile profile, int quality, DemandMultipliers demand)
        {
            float tierMultiplier = 1f + 0.25f * geneticsTier;
            float qualityMultiplier = 0.5f + quality / 100f;

            float demandScore = (
                (profile.Potency / 100f) * demand.Potency +
                (profile.Relaxation / 100f) * demand.Relaxation +
                (profile.Energy / 100f) * demand.Energy +
                (profile.Focus / 100f) * demand.Focus
            ) / 4f;

            return BasePrice * tierMultiplier * qualityMultiplier * demandScore;
        }
    }
}
