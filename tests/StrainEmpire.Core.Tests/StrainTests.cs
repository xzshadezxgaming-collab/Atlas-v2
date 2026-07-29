using StrainEmpire.Core.Genetics;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    public class StrainTests
    {
        [Theory]
        [InlineData(0, 0)]
        [InlineData(19, 0)]
        [InlineData(20, 1)]
        [InlineData(79, 3)]
        [InlineData(80, 4)]
        [InlineData(100, 4)]
        public void GeneticsTier_FloorsAverageTraitsDividedBy20(int traitValue, int expectedTier)
        {
            var strain = new Strain { Potency = traitValue, Yield = traitValue, Speed = traitValue, Resilience = traitValue };
            Assert.Equal(expectedTier, strain.GeneticsTier);
        }

        [Fact]
        public void GrowTimeHours_ScalesFrom4To24Hours()
        {
            var weak = new Strain { Potency = 0, Yield = 0, Speed = 0, Resilience = 0 };
            var maxed = new Strain { Potency = 100, Yield = 100, Speed = 100, Resilience = 100 };

            Assert.Equal(4f, weak.GrowTimeHours);
            Assert.Equal(24f, maxed.GrowTimeHours);
        }
    }
}
