using StrainEmpire.Core.Random;

namespace StrainEmpire.Core.Genetics
{
    /// systems-design.md: offspring trait/profile values average the two
    /// parents, then each has a 10% chance of a +/-(5-15) mutation, clamped
    /// to [0, 100]. Averaging biases offspring toward parent quality;
    /// mutation keeps outcomes non-deterministic enough to matter.
    public static class BreedingSystem
    {
        private const double MutationChance = 0.10;
        private const int MutationMagnitudeMin = 5;
        private const int MutationMagnitudeMax = 15; // inclusive

        public static Strain Breed(Strain parentA, Strain parentB, IRandomSource rng, string offspringId, string offspringName)
        {
            return new Strain
            {
                Id = offspringId,
                Name = offspringName,
                Potency = BreedValue(parentA.Potency, parentB.Potency, rng),
                Yield = BreedValue(parentA.Yield, parentB.Yield, rng),
                Speed = BreedValue(parentA.Speed, parentB.Speed, rng),
                Resilience = BreedValue(parentA.Resilience, parentB.Resilience, rng),
                BaselineRelaxation = BreedValue(parentA.BaselineRelaxation, parentB.BaselineRelaxation, rng),
                BaselineEnergy = BreedValue(parentA.BaselineEnergy, parentB.BaselineEnergy, rng),
                BaselineFocus = BreedValue(parentA.BaselineFocus, parentB.BaselineFocus, rng),
            };
        }

        private static int BreedValue(int a, int b, IRandomSource rng)
        {
            double average = (a + b) / 2.0;
            int value = (int)System.Math.Round(average, System.MidpointRounding.AwayFromZero);

            if (rng.NextDouble() < MutationChance)
            {
                int magnitude = rng.NextInt(MutationMagnitudeMin, MutationMagnitudeMax + 1);
                int sign = rng.NextDouble() < 0.5 ? -1 : 1;
                value += sign * magnitude;
            }

            return Clamp(value, 0, 100);
        }

        private static int Clamp(int value, int min, int max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }
    }
}
