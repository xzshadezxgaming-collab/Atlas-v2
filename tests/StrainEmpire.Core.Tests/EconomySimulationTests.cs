using System.Collections.Generic;
using StrainEmpire.Core.Economy;
using StrainEmpire.Core.Genetics;
using StrainEmpire.Core.Market;
using StrainEmpire.Core.Mixing;
using StrainEmpire.Core.Random;
using StrainEmpire.Core.Session;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    /// There's no Unity Editor available in this environment to playtest
    /// visually, so this simulates many in-game days of the full loop
    /// (plant -> grow -> harvest+mix+sell -> replant, weekly season
    /// rotation, plot purchases when affordable) as the closest available
    /// substitute: it exercises every system together and asserts the
    /// economy behaves sanely rather than degenerating or corrupting state.
    public class EconomySimulationTests
    {
        [Fact]
        public void Simulate30Days_EconomyGrowsAndStaysSane()
        {
            var rng = new SystemRandomSource(seed: 12345);
            var session = new GameSession(rng, SeasonArchetype.BalancedMarket);
            Strain starter = session.CreateStarterStrain("Starter", potency: 40, yield: 40, speed: 40, resilience: 40, relaxation: 20, energy: 20, focus: 20);
            session.Plant(0, starter);

            var noIngredients = new List<Ingredient>();
            var seasons = new[]
            {
                SeasonArchetype.BalancedMarket, SeasonArchetype.HeavyHitterWeek, SeasonArchetype.ChillWave,
                SeasonArchetype.RiseAndGrind, SeasonArchetype.ClaritySeason,
            };
            int seasonIndex = 0;
            float peakCash = session.Cash;

            for (int day = 0; day < 30; day++)
            {
                session.AdvanceTime(24f);

                for (int i = 0; i < session.Plots.Count; i++)
                {
                    if (!session.Plots[i].IsMature) continue;

                    float revenue = session.HarvestMixAndSell(i, noIngredients);
                    Assert.True(revenue >= 0f, $"Day {day}: revenue went negative ({revenue})");
                    Assert.False(float.IsNaN(revenue), $"Day {day}: revenue is NaN");

                    Strain toReplant = session.StrainInventory[session.StrainInventory.Count - 1];
                    session.Plant(i, toReplant);
                }

                session.BuyPlot(); // expand whenever affordable, no-op otherwise

                if (day % 7 == 0)
                {
                    seasonIndex = (seasonIndex + 1) % seasons.Length;
                    session.AdvanceSeason(seasons[seasonIndex]);
                }

                Assert.True(session.Cash >= 0f, $"Day {day}: cash went negative ({session.Cash})");
                Assert.False(float.IsNaN(session.Cash), $"Day {day}: cash is NaN");
                if (session.Cash > peakCash) peakCash = session.Cash;
            }

            Assert.True(
                peakCash > EconomyConfig.StartingCash,
                $"Expected cash to grow above the starting {EconomyConfig.StartingCash} over 30 days, peaked at {peakCash}");
        }
    }
}
