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
