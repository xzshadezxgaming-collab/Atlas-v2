namespace StrainEmpire.Core.Market
{
    public enum EffectTag
    {
        Potency,
        Relaxation,
        Energy,
        Focus,
    }

    public struct DemandMultipliers
    {
        public float Potency;
        public float Relaxation;
        public float Energy;
        public float Focus;
    }

    /// One archetype rolls per 7-day season. See docs/systems-design.md
    /// "Seasonal Market" for the multiplier table.
    public enum SeasonArchetype
    {
        HeavyHitterWeek,
        ChillWave,
        RiseAndGrind,
        ClaritySeason,
        BalancedMarket,
        VolatileSwing,
    }
}
