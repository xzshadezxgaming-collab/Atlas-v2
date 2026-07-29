using StrainEmpire.Core.Genetics;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    public class GrowPlotTests
    {
        private static Strain MakeStrain(int growTraitValue)
        {
            return new Strain { Potency = growTraitValue, Yield = growTraitValue, Speed = growTraitValue, Resilience = growTraitValue };
        }

        [Fact]
        public void Harvest_BeforeMaturity_ReturnsNull()
        {
            var plot = new GrowPlot();
            plot.Plant(MakeStrain(50)); // GrowTimeHours = 4 + (50/100)*20 = 14

            plot.Advance(10f);

            Assert.False(plot.IsMature);
            Assert.Null(plot.Harvest());
        }

        [Fact]
        public void Harvest_AfterMaturity_ReturnsStrainAndClearsPlot()
        {
            var plot = new GrowPlot();
            Strain strain = MakeStrain(50); // GrowTimeHours = 14
            plot.Plant(strain);

            plot.Advance(20f); // simulates offline catch-up past maturity

            Assert.True(plot.IsMature);
            Strain harvested = plot.Harvest();

            Assert.Same(strain, harvested);
            Assert.False(plot.IsPlanted);
        }
    }
}
