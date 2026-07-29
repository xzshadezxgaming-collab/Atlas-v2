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

        /// Whether a Breed() call would currently succeed (see BreedingCooldownHours).
        public bool CanBreed => _breedingCooldownRemainingHours <= 0f;

        private readonly IRandomSource _rng;
        private int _nextStrainId = 1;
        private float _breedingCooldownRemainingHours;

        public const float BreedingCooldownHours = 12f; // docs/systems-design.md "Breeding cooldown"
        public const int SeedsPerBreeding = 3; // docs/systems-design.md "a seed batch (3 seeds)"

        public GameSession(IRandomSource rng, SeasonArchetype startingSeason)
        {
            _rng = rng;
            Cash = EconomyConfig.StartingCash;
            Plots.Add(new GrowPlot()); // 1 free starting plot
            CurrentSeason = startingSeason;
            CurrentDemand = MarketSystem.GetDemandMultipliers(startingSeason, _rng);
        }

        /// Rebuilds a session from saved state (see Gameplay save system),
        /// bypassing the "1 free starting plot / starting cash" defaults the
        /// normal constructor applies.
        public static GameSession Restore(
            IRandomSource rng,
            float cash,
            IEnumerable<GrowPlot> plots,
            IEnumerable<Strain> strainInventory,
            SeasonArchetype currentSeason,
            int nextStrainId)
        {
            var session = new GameSession(rng, currentSeason)
            {
                Cash = cash,
            };
            session.Plots.Clear();
            session.Plots.AddRange(plots);
            session.StrainInventory.Clear();
            session.StrainInventory.AddRange(strainInventory);
            session._nextStrainId = nextStrainId;
            return session;
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

            if (_breedingCooldownRemainingHours > 0f)
                _breedingCooldownRemainingHours = System.Math.Max(0f, _breedingCooldownRemainingHours - hours);
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

        /// Produces SeedsPerBreeding independently-rolled offspring (each
        /// with its own mutation roll) so the player has a pool to select
        /// the best mutant from each generation — that selection is what
        /// gives genetics real upward progress over time, not a guaranteed
        /// single averaged result. Returns null if a breeding cooldown is
        /// still active (see BreedingCooldownHours / CanBreed) — check
        /// CanBreed before calling if the UI needs to disable the action
        /// rather than silently no-op.
        public IReadOnlyList<Strain> Breed(Strain parentA, Strain parentB, string offspringNamePrefix)
        {
            if (!CanBreed) return null;

            var seeds = new List<Strain>(SeedsPerBreeding);
            for (int i = 0; i < SeedsPerBreeding; i++)
            {
                Strain seed = BreedingSystem.Breed(parentA, parentB, _rng, $"strain-{_nextStrainId++}", $"{offspringNamePrefix} #{i + 1}");
                StrainInventory.Add(seed);
                seeds.Add(seed);
            }

            _breedingCooldownRemainingHours = BreedingCooldownHours;
            return seeds;
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
