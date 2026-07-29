using System.Collections.Generic;
using System.Linq;
using StrainEmpire.Core.Economy;
using StrainEmpire.Core.Genetics;
using StrainEmpire.Core.Market;
using StrainEmpire.Core.Mixing;
using StrainEmpire.Core.Random;

namespace StrainEmpire.Core.Session
{
    /// Owns all mutable game state and exposes the player-facing actions.
    /// Pure C# (no UnityEngine dependency), so the whole loop is unit
    /// testable; Assets/Scripts/Gameplay/GameManager.cs is a thin Unity
    /// wrapper around this.
    public class GameSession
    {
        public float Cash { get; private set; }
        public List<GrowPlot> Plots { get; } = new List<GrowPlot>();
        public List<Strain> StrainInventory { get; } = new List<Strain>();
        public SeasonArchetype CurrentSeason { get; private set; }
        public DemandMultipliers CurrentDemand { get; private set; }

        private readonly IRandomSource _rng;
        private int _nextStrainId = 1;

        public GameSession(IRandomSource rng, SeasonArchetype startingSeason)
        {
            _rng = rng;
            Cash = EconomyConfig.StartingCash;
            Plots.Add(new GrowPlot()); // 1 free starting plot
            CurrentSeason = startingSeason;
            CurrentDemand = MarketSystem.GetDemandMultipliers(startingSeason, _rng);
        }

        public Strain CreateStarterStrain(string name, int potency, int yield, int speed, int resilience, int relaxation, int energy, int focus)
        {
            var strain = new Strain
            {
                Id = $"strain-{_nextStrainId++}",
                Name = name,
                Potency = potency,
                Yield = yield,
                Speed = speed,
                Resilience = resilience,
                BaselineRelaxation = relaxation,
                BaselineEnergy = energy,
                BaselineFocus = focus,
            };
            StrainInventory.Add(strain);
            return strain;
        }

        /// Cost follows EconomyConfig.PlotCost(n) where n is the plot about
        /// to be added (the free starting plot doesn't count).
        public bool BuyPlot()
        {
            int n = Plots.Count;
            float cost = EconomyConfig.PlotCost(n);
            if (Cash < cost) return false;

            Cash -= cost;
            Plots.Add(new GrowPlot());
            return true;
        }

        public bool Plant(int plotIndex, Strain strain)
        {
            if (plotIndex < 0 || plotIndex >= Plots.Count) return false;

            GrowPlot plot = Plots[plotIndex];
            if (plot.IsPlanted) return false;
            if (!StrainInventory.Remove(strain)) return false;

            plot.Plant(strain);
            return true;
        }

        /// Feed this real-time delta hours for live ticking, or a larger
        /// value (capped via OfflineProgressCalculator) to catch up offline time.
        public void AdvanceTime(float hours)
        {
            foreach (GrowPlot plot in Plots)
                plot.Advance(hours);
        }

        /// Harvests a mature plot, mixes with the given ingredients, and
        /// immediately sells the batch at the current season's demand. The
        /// strain (genetics) returns to inventory afterward — harvesting
        /// consumes the crop, not the strain, so it can be replanted or bred.
        /// Batch size scales with the strain's Yield trait (5-15 units) per
        /// docs/systems-design.md.
        public float HarvestMixAndSell(int plotIndex, IReadOnlyList<Ingredient> ingredients)
        {
            if (plotIndex < 0 || plotIndex >= Plots.Count) return 0f;

            GrowPlot plot = Plots[plotIndex];
            Strain harvested = plot.Harvest();
            if (harvested == null) return 0f;

            MixResult mix = MixingSystem.Mix(harvested, ingredients);
            float unitPrice = MarketSystem.ComputeUnitPrice(harvested.GeneticsTier, mix.Profile, mix.Quality, CurrentDemand);
            float baseBatchSize = 5f + (harvested.Yield / 100f) * 10f;
            float revenue = unitPrice * baseBatchSize * mix.YieldMultiplier;

            Cash += revenue;
            StrainInventory.Add(harvested);
            return revenue;
        }

        public Strain Breed(Strain parentA, Strain parentB, string offspringName)
        {
            Strain offspring = BreedingSystem.Breed(parentA, parentB, _rng, $"strain-{_nextStrainId++}", offspringName);
            StrainInventory.Add(offspring);
            return offspring;
        }

        public void AdvanceSeason(SeasonArchetype nextArchetype)
        {
            CurrentSeason = nextArchetype;
            CurrentDemand = MarketSystem.GetDemandMultipliers(nextArchetype, _rng);
        }

        /// The Steam Leaderboard score. Counts strains both in inventory and
        /// currently planted.
        public float ComputeEmpireValue()
        {
            float plotValue = 0f;
            for (int n = 1; n < Plots.Count; n++)
                plotValue += EconomyConfig.PlotCost(n);

            IEnumerable<Strain> allStrains = StrainInventory
                .Concat(Plots.Where(p => p.IsPlanted).Select(p => p.PlantedStrain));

            return EmpireValueCalculator.Compute(Cash, plotValue, allStrains);
        }
    }
}
