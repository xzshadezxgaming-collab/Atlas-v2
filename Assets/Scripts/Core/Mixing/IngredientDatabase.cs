using System.Collections.Generic;

namespace StrainEmpire.Core.Mixing
{
    /// The 10-ingredient table from docs/systems-design.md.
    public static class IngredientDatabase
    {
        public static readonly IReadOnlyList<Ingredient> All = new List<Ingredient>
        {
            new Ingredient { Name = "Sunroot Extract", EnergyDelta = 15, RelaxationDelta = -5, Cost = 8, UnlockTier = 0 },
            new Ingredient { Name = "Mossveil Leaf", RelaxationDelta = 15, EnergyDelta = -5, Cost = 8, UnlockTier = 0 },
            new Ingredient { Name = "Clarid Crystal", FocusDelta = 12, PotencyDelta = -8, Cost = 10, UnlockTier = 0 },
            new Ingredient { Name = "Emberash Resin", PotencyDelta = 18, FocusDelta = -6, Cost = 14, UnlockTier = 0 },
            new Ingredient { Name = "Silverdew Drops", QualityDelta = 8, Cost = 12, UnlockTier = 0 },
            new Ingredient { Name = "Thistlebind Fiber", YieldPercentDelta = 0.10f, QualityDelta = -4, Cost = 6, UnlockTier = 0 },
            new Ingredient { Name = "Glowcap Spores", PotencyDelta = 10, EnergyDelta = 10, RelaxationDelta = -10, Cost = 16, UnlockTier = 1 },
            new Ingredient { Name = "Duskmint", RelaxationDelta = 10, FocusDelta = 10, EnergyDelta = -8, Cost = 15, UnlockTier = 1 },
            new Ingredient { Name = "Ironbark Ash", PotencyDelta = 6, QualityDelta = 6, Cost = 18, UnlockTier = 2 },
            new Ingredient { Name = "Prism Dew", PotencyDelta = 12, RelaxationDelta = 12, EnergyDelta = 12, FocusDelta = 12, QualityDelta = -15, Cost = 20, UnlockTier = 2 },
        };
    }
}
