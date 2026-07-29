using System.Collections.Generic;

namespace StrainEmpire.Gameplay.Store
{
    /// Placeholder catalog matching docs/systems-design.md "Gems &
    /// Speedups". Actual prices are set in App Store Connect / Play
    /// Console, not in code — ProductId here must match the product IDs
    /// configured there exactly.
    public static class GemPackCatalog
    {
        public static readonly IReadOnlyList<GemPack> All = new List<GemPack>
        {
            new GemPack { ProductId = "gems_small", DisplayName = "Small Gem Pack (100)", GemAmount = 100 },
            new GemPack { ProductId = "gems_medium", DisplayName = "Medium Gem Pack (550)", GemAmount = 550 },
            new GemPack { ProductId = "gems_large", DisplayName = "Large Gem Pack (1200)", GemAmount = 1200 },
        };
    }
}
