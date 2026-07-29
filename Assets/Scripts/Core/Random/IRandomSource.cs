namespace StrainEmpire.Core.Random
{
    /// Injected so breeding/market rolls are deterministic under test.
    public interface IRandomSource
    {
        int NextInt(int minInclusive, int maxExclusive);
        double NextDouble();
    }
}
