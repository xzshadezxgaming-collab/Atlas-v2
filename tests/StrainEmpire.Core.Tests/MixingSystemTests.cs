using System.Collections.Generic;
using System.Linq;
using StrainEmpire.Core.Genetics;
using StrainEmpire.Core.Mixing;
using Xunit;

namespace StrainEmpire.Core.Tests
{
    public class MixingSystemTests
    {
        private static Strain MakeStrain(int potency, int relaxation, int energy, int focus)
        {
            return new Strain
            {
                Potency = potency,
                BaselineRelaxation = relaxation,
                BaselineEnergy = energy,
                BaselineFocus = focus,
            };
        }

        private static Ingredient Find(string name) => IngredientDatabase.All.Single(i => i.Name == name);

        [Fact]
        public void Mix_AppliesIngredientDeltasAndBaseQuality()
        {
            Strain strain = MakeStrain(potency: 50, relaxation: 30, energy: 20, focus: 10);
            var ingredients = new List<Ingredient> { Find("Sunroot Extract"), Find("Clarid Crystal") };

            MixResult result = MixingSystem.Mix(strain, ingredients);

            Assert.Equal(42, result.Profile.Potency);    // 50 - 8 (Clarid)
            Assert.Equal(25, result.Profile.Relaxation); // 30 - 5 (Sunroot)
            Assert.Equal(35, result.Profile.Energy);     // 20 + 15 (Sunroot)
            Assert.Equal(22, result.Profile.Focus);      // 10 + 12 (Clarid)
            Assert.Equal(50, result.Quality);            // base only, neither ingredient touches quality
            Assert.Equal(1.0f, result.YieldMultiplier);
        }

        [Fact]
        public void Mix_ClampsEffectTagsAndQualityTo100()
        {
            Strain strain = MakeStrain(potency: 95, relaxation: 0, energy: 0, focus: 0);
            var ingredients = new List<Ingredient> { Find("Emberash Resin"), Find("Ironbark Ash") };

            MixResult result = MixingSystem.Mix(strain, ingredients);

            Assert.Equal(100, result.Profile.Potency); // 95 + 18 + 6 = 119 -> clamped
            Assert.Equal(56, result.Quality);           // 50 + 6 (Ironbark)
        }

        [Fact]
        public void Mix_MoreThanMaxSlots_Throws()
        {
            Strain strain = MakeStrain(50, 50, 50, 50);
            List<Ingredient> ingredients = IngredientDatabase.All.Take(4).ToList();

            Assert.Throws<System.ArgumentException>(() => MixingSystem.Mix(strain, ingredients));
        }
    }
}
