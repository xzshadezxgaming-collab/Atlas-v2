using StrainEmpire.Core.Genetics;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    public class BreedingSystemTests
    {
        private static Strain MakeStrain(int potency, int yield, int speed, int resilience, int relaxation, int energy, int focus)
        {
            return new Strain
            {
                Potency = potency,
                Yield = yield,
                Speed = speed,
                Resilience = resilience,
                BaselineRelaxation = relaxation,
                BaselineEnergy = energy,
                BaselineFocus = focus,
            };
        }

        [Fact]
        public void Breed_NoMutation_ReturnsExactAverageOfParents()
        {
            Strain parentA = MakeStrain(40, 60, 50, 30, 20, 10, 15);
            Strain parentB = MakeStrain(60, 40, 30, 50, 40, 30, 25);

            // NextDouble always 0.99: every mutation-chance roll (< 0.10) fails.
            var rng = new FakeRandomSource(new[] { 0.99 });

            Strain offspring = BreedingSystem.Breed(parentA, parentB, rng, "seed-1", "Test Seed");

            Assert.Equal(50, offspring.Potency);
            Assert.Equal(50, offspring.Yield);
            Assert.Equal(40, offspring.Speed);
            Assert.Equal(40, offspring.Resilience);
            Assert.Equal(30, offspring.BaselineRelaxation);
            Assert.Equal(20, offspring.BaselineEnergy);
            Assert.Equal(20, offspring.BaselineFocus);
        }

        [Fact]
        public void Breed_AlwaysMutatesUpward_ClampsAt100()
        {
            Strain parentA = MakeStrain(90, 50, 50, 50, 50, 50, 50);
            Strain parentB = MakeStrain(100, 50, 50, 50, 50, 50, 50);

            // Cycle [0.05 (mutation triggers, <0.10), 0.9 (positive sign, >=0.5)]
            // repeats per trait; NextInt always returns 15 (max magnitude).
            var rng = new FakeRandomSource(new[] { 0.05, 0.9 }, fixedInt: 15);

            Strain offspring = BreedingSystem.Breed(parentA, parentB, rng, "seed-2", "Test Seed 2");

            Assert.Equal(100, offspring.Potency); // avg 95 + 15 = 110 -> clamped
            Assert.Equal(65, offspring.Yield);    // avg 50 + 15 = 65, no clamp needed
        }

        [Fact]
        public void Breed_AlwaysMutatesDownward_ClampsAt0()
        {
            Strain parentA = MakeStrain(5, 50, 50, 50, 50, 50, 50);
            Strain parentB = MakeStrain(0, 50, 50, 50, 50, 50, 50);

            // Cycle [0.05 (mutation triggers), 0.1 (negative sign, <0.5)]
            // repeats per trait; NextInt always returns 15.
            var rng = new FakeRandomSource(new[] { 0.05, 0.1 }, fixedInt: 15);

            Strain offspring = BreedingSystem.Breed(parentA, parentB, rng, "seed-3", "Test Seed 3");

            Assert.Equal(0, offspring.Potency); // avg 2.5 -> rounds to 3, -15 = -12 -> clamped
            Assert.Equal(35, offspring.Yield);  // avg 50 - 15 = 35
        }
    }
}
