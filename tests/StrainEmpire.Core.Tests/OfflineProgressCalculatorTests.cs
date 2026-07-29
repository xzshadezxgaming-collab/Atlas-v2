using StrainEmpire.Core.Economy;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    public class OfflineProgressCalculatorTests
    {
        [Fact]
        public void ComputeCatchUpHours_BelowCap_ReturnsElapsedHours()
        {
            float hours = OfflineProgressCalculator.ComputeCatchUpHours(elapsedRealSeconds: 3600 * 5, offlineCapHours: 8);
            Assert.Equal(5f, hours);
        }

        [Fact]
        public void ComputeCatchUpHours_AboveCap_ReturnsCap()
        {
            float hours = OfflineProgressCalculator.ComputeCatchUpHours(elapsedRealSeconds: 3600 * 30, offlineCapHours: 8);
            Assert.Equal(8f, hours);
        }

        [Fact]
        public void ComputeCatchUpHours_NegativeElapsed_ReturnsZero()
        {
            // Guards against clock skew (system clock moved backward).
            float hours = OfflineProgressCalculator.ComputeCatchUpHours(elapsedRealSeconds: -100, offlineCapHours: 8);
            Assert.Equal(0f, hours);
        }
    }
}
