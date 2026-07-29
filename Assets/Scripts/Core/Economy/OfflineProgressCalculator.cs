namespace StrainEmpire.Core.Economy
{
    /// Converts real elapsed time while the app was closed into in-game
    /// catch-up hours, capped by the offline cap (EconomyConfig).
    public static class OfflineProgressCalculator
    {
        public static float ComputeCatchUpHours(double elapsedRealSeconds, int offlineCapHours)
        {
            if (elapsedRealSeconds <= 0) return 0f;

            float elapsedHours = (float)(elapsedRealSeconds / 3600.0);
            return elapsedHours > offlineCapHours ? offlineCapHours : elapsedHours;
        }
    }
}
