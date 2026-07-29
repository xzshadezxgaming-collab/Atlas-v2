using System.Collections.Generic;
using StrainEmpire.Core.Genetics;
using StrainEmpire.Core.Market;
using StrainEmpire.Core.Mixing;
using StrainEmpire.Core.Session;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    public class GameSessionTests
    {
        [Fact]
        public void Constructor_SetsUpStartingCashAndOnePlot()
        {
            var session = new GameSession(new FakeRandomSource(new[] { 0.99 }), SeasonArchetype.HeavyHitterWeek);

            Assert.Equal(200f, session.Cash);
            Assert.Single(session.Plots);
            Assert.Equal(1.8f, session.CurrentDemand.Potency);
        }

        [Fact]
        public void BuyPlot_DeductsCorrectCostAndAddsPlot_FailsWhenCantAfford()
        {
            var session = new GameSession(new FakeRandomSource(new[] { 0.99 }), SeasonArchetype.HeavyHitterWeek);

            bool first = session.BuyPlot(); // n=1 -> cost 150
            Assert.True(first);
            Assert.Equal(50f, session.Cash);
            Assert.Equal(2, session.Plots.Count);

            bool second = session.BuyPlot(); // n=2 -> cost 240, only have 50
            Assert.False(second);
            Assert.Equal(50f, session.Cash);
            Assert.Equal(2, session.Plots.Count);
        }

        [Fact]
        public void PlantHarvestMixSell_ReturnsStrainToInventoryAndPaysExpectedRevenue()
        {
            var session = new GameSession(new FakeRandomSource(new[] { 0.99 }), SeasonArchetype.HeavyHitterWeek);
            Strain strain = session.CreateStarterStrain("Test Strain", potency: 100, yield: 100, speed: 100, resilience: 100, relaxation: 0, energy: 0, focus: 0);

            Assert.True(session.Plant(0, strain));
            Assert.Empty(session.StrainInventory);

            session.AdvanceTime(24f); // GrowTimeHours for a maxed strain = 24

            float revenue = session.HarvestMixAndSell(0, new List<Ingredient>());

            // geneticsTier=4 -> tierMultiplier=2.0; quality=50 (base) -> qualityMultiplier=1.0
            // demandScore = (1.0*1.8)/4 = 0.45; unitPrice = 5*2.0*1.0*0.45 = 4.5
            // baseBatchSize = 5 + (100/100)*10 = 15; revenue = 4.5*15*1.0 = 67.5
            Assert.True(System.Math.Abs(revenue - 67.5f) < 0.01f, $"Expected ~67.5, got {revenue}");
            Assert.True(System.Math.Abs(session.Cash - 267.5f) < 0.01f, $"Expected ~267.5, got {session.Cash}");
            Assert.Single(session.StrainInventory); // strain returned after harvest
            Assert.False(session.Plots[0].IsPlanted);
        }

        [Fact]
        public void Breed_NoMutation_AddsAveragedOffspringToInventory()
        {
            var session = new GameSession(new FakeRandomSource(new[] { 0.99 }), SeasonArchetype.HeavyHitterWeek);
            Strain parentA = session.CreateStarterStrain("A", 40, 60, 50, 30, 20, 10, 15);
            Strain parentB = session.CreateStarterStrain("B", 60, 40, 30, 50, 40, 30, 25);

            Strain offspring = session.Breed(parentA, parentB, "Child");

            Assert.Equal(50, offspring.Potency);
            Assert.Contains(offspring, session.StrainInventory);
        }

        [Fact]
        public void AdvanceSeason_UpdatesCurrentSeasonAndDemand()
        {
            var session = new GameSession(new FakeRandomSource(new[] { 0.99 }), SeasonArchetype.HeavyHitterWeek);

            session.AdvanceSeason(SeasonArchetype.ChillWave);

            Assert.Equal(SeasonArchetype.ChillWave, session.CurrentSeason);
            Assert.Equal(1.8f, session.CurrentDemand.Relaxation);
        }

        [Fact]
        public void ComputeEmpireValue_CountsCashPlotsAndPlantedAndInventoryStrains()
        {
            var session = new GameSession(new FakeRandomSource(new[] { 0.99 }), SeasonArchetype.HeavyHitterWeek);
            session.BuyPlot(); // Cash 200 -> 50, +1 plot (cost 150)

            Strain planted = session.CreateStarterStrain("Planted", 80, 80, 80, 80, 0, 0, 0); // tier 4
            Strain inInventory = session.CreateStarterStrain("Bench", 20, 20, 20, 20, 0, 0, 0); // tier 1
            session.Plant(0, planted);

            float value = session.ComputeEmpireValue();

            // cash 50 + plotValue 150 + (4*500) + (1*500) = 50 + 150 + 2000 + 500 = 2700
            Assert.True(System.Math.Abs(value - 2700f) < 0.01f, $"Expected ~2700, got {value}");
        }

        [Fact]
        public void Restore_RebuildsSessionExactlyFromSavedState()
        {
            var original = new GameSession(new FakeRandomSource(new[] { 0.99 }), SeasonArchetype.ChillWave);
            original.BuyPlot();
            Strain planted = original.CreateStarterStrain("Planted", 70, 60, 50, 40, 30, 20, 10);
            Strain bench = original.CreateStarterStrain("Bench", 10, 10, 10, 10, 10, 10, 10);
            original.Plant(0, planted);
            original.AdvanceTime(5f);

            var restored = GameSession.Restore(
                new FakeRandomSource(new[] { 0.99 }),
                original.Cash,
                original.Plots,
                original.StrainInventory,
                original.CurrentSeason,
                nextStrainId: 99);

            Assert.Equal(original.Cash, restored.Cash);
            Assert.Equal(original.Plots.Count, restored.Plots.Count);
            Assert.True(restored.Plots[0].IsPlanted);
            Assert.Equal(5f, restored.Plots[0].ElapsedHours);
            Assert.Equal(original.StrainInventory.Count, restored.StrainInventory.Count);
            Assert.Equal(SeasonArchetype.ChillWave, restored.CurrentSeason);

            Strain nextOffspring = restored.Breed(planted, bench, "Next");
            Assert.Equal("strain-99", nextOffspring.Id);
        }
    }
}
