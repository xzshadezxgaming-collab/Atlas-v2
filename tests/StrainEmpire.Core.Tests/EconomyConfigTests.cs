using StrainEmpire.Core.Economy;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    public class EconomyConfigTests
    {
        [Theory]
        [InlineData(1, 150f)]
        [InlineData(2, 240f)]
        [InlineData(3, 384f)]
        public void PlotCost_MatchesGeometricCurve(int n, float expected)
        {
            float cost = EconomyConfig.PlotCost(n);
            Assert.True(System.Math.Abs(cost - expected) < 0.01f, $"Expected ~{expected}, got {cost}");
        }
    }
}
