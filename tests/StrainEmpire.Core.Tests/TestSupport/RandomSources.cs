using System.Collections.Generic;
using StrainEmpire.Core.Random;

namespace StrainEmpire.Core.Tests
{
    /// Cycles through a fixed NextDouble() sequence; NextInt() always
    /// returns the same fixed value. Used to make breeding/mutation rolls
    /// deterministic in tests.
    public class FakeRandomSource : IRandomSource
    {
        private readonly IReadOnlyList<double> _doubles;
        private readonly int _fixedInt;
        private int _doubleIndex;

        public FakeRandomSource(IReadOnlyList<double> doubles, int fixedInt = 0)
        {
            _doubles = doubles;
            _fixedInt = fixedInt;
        }

        public double NextDouble()
        {
            double value = _doubles[_doubleIndex % _doubles.Count];
            _doubleIndex++;
            return value;
        }

        public int NextInt(int minInclusive, int maxExclusive) => _fixedInt;
    }

    /// Cycles through a fixed NextInt() sequence; NextDouble() is unused by
    /// callers of this fake and returns a constant.
    public class SequencedIntRandomSource : IRandomSource
    {
        private readonly IReadOnlyList<int> _ints;
        private int _intIndex;

        public SequencedIntRandomSource(IReadOnlyList<int> ints)
        {
            _ints = ints;
        }

        public int NextInt(int minInclusive, int maxExclusive)
        {
            int value = _ints[_intIndex % _ints.Count];
            _intIndex++;
            return value;
        }

        public double NextDouble() => 0.5;
    }
}
