using StrainEmpire.Core.Genetics;

namespace StrainEmpire.Core
{
    /// A single plantable plot. Advance() is fed elapsed real/offline hours
    /// so the same call drives both live ticking and offline catch-up.
    public class GrowPlot
    {
        public Strain PlantedStrain { get; private set; }
        public float ElapsedHours { get; private set; }

        public bool IsPlanted => PlantedStrain != null;
        public bool IsMature => IsPlanted && ElapsedHours >= PlantedStrain.GrowTimeHours;

        public void Plant(Strain strain)
        {
            PlantedStrain = strain;
            ElapsedHours = 0f;
        }

        /// Rebuilds a plot from saved state (see Gameplay save system).
        /// Pass a null strain for an empty plot.
        public static GrowPlot Restore(Strain plantedStrain, float elapsedHours)
        {
            var plot = new GrowPlot();
            if (plantedStrain != null)
            {
                plot.PlantedStrain = plantedStrain;
                plot.ElapsedHours = elapsedHours;
            }
            return plot;
        }

        public void Advance(float hours)
        {
            if (!IsPlanted) return;
            ElapsedHours += hours;
        }

        /// Clears the plot and returns the grown strain, or null if not yet mature.
        public Strain Harvest()
        {
            if (!IsMature) return null;

            Strain strain = PlantedStrain;
            PlantedStrain = null;
            ElapsedHours = 0f;
            return strain;
        }
    }
}
