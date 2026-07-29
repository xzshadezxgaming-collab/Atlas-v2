namespace StrainEmpire.Core.Genetics
{
    /// A cultivated strain: 4 growth traits plus a baseline effect profile,
    /// both inherited on breeding. See docs/systems-design.md for the formulas.
    public class Strain
    {
        public string Id { get; set; }
        public string Name { get; set; }

        // Growth traits, 0-100.
        public int Potency { get; set; }
        public int Yield { get; set; }
        public int Speed { get; set; }
        public int Resilience { get; set; }

        // Baseline effect profile, 0-100. Potency's market tag is covered by
        // the Potency trait above; these three are the strain's "character"
        // before any ingredient mixing is applied.
        public int BaselineRelaxation { get; set; }
        public int BaselineEnergy { get; set; }
        public int BaselineFocus { get; set; }

        public float AverageTraits => (Potency + Yield + Speed + Resilience) / 4f;

        // systems-design.md: floor(average traits / 20), clamped to [0, 4].
        public int GeneticsTier
        {
            get
            {
                int tier = (int)(AverageTraits / 20f);
                return tier > 4 ? 4 : tier;
            }
        }

        // systems-design.md: 4 + (avg traits / 100) * 20 hours.
        public float GrowTimeHours => 4f + (AverageTraits / 100f) * 20f;
    }
}
