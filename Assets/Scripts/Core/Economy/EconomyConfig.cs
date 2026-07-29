using System;

namespace StrainEmpire.Core.Economy
{
    public static class EconomyConfig
    {
        public const int StartingCash = 200;
        public const float PlotCostBase = 150f;
        public const float PlotCostGrowth = 1.6f;
        public const int OfflineCapBaseHours = 8;
        public const int OfflineCapExtendedHours = 24; // via the QoL MTX pass

        // Gems & Speedups (docs/systems-design.md)
        public const int GemsPerRewardedAd = 20;
        public const int MaxRewardedAdsPerDay = 5;
        public const float InstantGrowGemsCostPerHour = 5f;

        /// Cost of the nth additional plot beyond the free starting plot (n = 1, 2, 3, ...).
        public static float PlotCost(int n) => PlotCostBase * (float)Math.Pow(PlotCostGrowth, n - 1);
    }
}
