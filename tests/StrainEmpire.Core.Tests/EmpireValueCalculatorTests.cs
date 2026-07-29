using System.Collections.Generic;
using StrainEmpire.Core.Economy;
using StrainEmpire.Core.Genetics;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    public class EmpireValueCalculatorTests
    {
        [Fact]
        public void Compute_SumsCashPlotsAndGeneticsTierValue()
        {
            var strains = new List<Strain>
            {
                new Strain { Potency = 80, Yield = 80, Speed = 80, Resilience = 80 }, // tier 4
                new Strain { Potency = 20, Yield = 20, Speed = 20, Resilience = 20 }, // tier 1
            };

            float value = EmpireValueCalculator.Compute(cashOnHand: 1000f, totalPlotValue: 500f, ownedStrains: strains);

            // 1000 + 500 + (4*500) + (1*500) = 4000
            Assert.Equal(4000f, value);
        }
    }
}
