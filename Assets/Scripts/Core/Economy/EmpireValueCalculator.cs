using System.Collections.Generic;
using StrainEmpire.Core.Genetics;

namespace StrainEmpire.Core.Economy
{
    /// The Steam Leaderboard score. See docs/systems-design.md "Leaderboard Score".
    public static class EmpireValueCalculator
    {
        public const int ValuePerGeneticsTier = 500;

        public static float Compute(float cashOnHand, float totalPlotValue, IEnumerable<Strain> ownedStrains)
        {
            float geneticsValue = 0f;
            foreach (Strain strain in ownedStrains)
                geneticsValue += strain.GeneticsTier * ValuePerGeneticsTier;

            return cashOnHand + totalPlotValue + geneticsValue;
        }
    }
}
