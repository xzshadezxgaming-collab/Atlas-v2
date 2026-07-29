using StrainEmpire.Core.Market;
using StrainEmpire.Core.Mixing;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    public class MarketSystemTests
    {
        [Fact]
        public void ComputeUnitPrice_MatchesDesignFormula()
        {
            var profile = new EffectProfile { Potency = 100, Relaxation = 0, Energy = 0, Focus = 0 };
            DemandMultipliers demand = MarketSystem.GetDemandMultipliers(SeasonArchetype.HeavyHitterWeek);

            float price = MarketSystem.ComputeUnitPrice(geneticsTier: 2, profile: profile, quality: 100, demand: demand);

            // tierMultiplier = 1 + 0.25*2 = 1.5; qualityMultiplier = 0.5 + 100/100 = 1.5
            // demandScore = (1.0*1.8 + 0 + 0 + 0) / 4 = 0.45
            // price = 5 * 1.5 * 1.5 * 0.45 = 5.0625
            Assert.True(System.Math.Abs(price - 5.0625f) < 0.001f, $"Expected ~5.0625, got {price}");
        }

        [Theory]
        [InlineData(SeasonArchetype.HeavyHitterWeek)]
        [InlineData(SeasonArchetype.ChillWave)]
        [InlineData(SeasonArchetype.RiseAndGrind)]
        [InlineData(SeasonArchetype.ClaritySeason)]
        public void GetDemandMultipliers_SingleTagArchetypes_BoostExactlyOneTag(SeasonArchetype archetype)
        {
            DemandMultipliers demand = MarketSystem.GetDemandMultipliers(archetype);
            float[] values = { demand.Potency, demand.Relaxation, demand.Energy, demand.Focus };

            Assert.Contains(1.8f, values);
            Assert.Equal(3, System.Array.FindAll(values, v => v == 0.8f).Length);
        }

        [Fact]
        public void GetDemandMultipliers_VolatileSwing_BoostsExactlyTwoTags()
        {
            // Tag order in MarketSystem is [Potency, Relaxation, Energy, Focus];
            // indices 0 then 1 pick Potency and Relaxation as the boosted pair.
            var rng = new SequencedIntRandomSource(new[] { 0, 1 });

            DemandMultipliers demand = MarketSystem.GetDemandMultipliers(SeasonArchetype.VolatileSwing, rng);

            Assert.Equal(1.5f, demand.Potency);
            Assert.Equal(1.5f, demand.Relaxation);
            Assert.Equal(0.6f, demand.Energy);
            Assert.Equal(0.6f, demand.Focus);
        }
    }
}
