namespace StrainEmpire.Core.Random
{
    public class SystemRandomSource : IRandomSource
    {
        private readonly System.Random _random;

        public SystemRandomSource(int? seed = null)
        {
            _random = seed.HasValue ? new System.Random(seed.Value) : new System.Random();
        }

        public int NextInt(int minInclusive, int maxExclusive) => _random.Next(minInclusive, maxExclusive);

        public double NextDouble() => _random.NextDouble();
    }
}
