namespace StrainEmpire.Core.Mixing
{
    public class Ingredient
    {
        public string Name { get; set; }
        public int PotencyDelta { get; set; }
        public int RelaxationDelta { get; set; }
        public int EnergyDelta { get; set; }
        public int FocusDelta { get; set; }
        public int QualityDelta { get; set; }

        /// e.g. 0.10 means +10% batch yield.
        public float YieldPercentDelta { get; set; }

        public int Cost { get; set; }

        /// Minimum genetics tier (0-4) required to unlock this ingredient.
        public int UnlockTier { get; set; }
    }
}
