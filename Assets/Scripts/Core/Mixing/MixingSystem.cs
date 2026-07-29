using System;
using System.Collections.Generic;
using StrainEmpire.Core.Genetics;

namespace StrainEmpire.Core.Mixing
{
    public class MixResult
    {
        public EffectProfile Profile;
        public int Quality;
        public float YieldMultiplier;
    }

    public static class MixingSystem
    {
        public const int MaxIngredientSlots = 3;
        public const int BaseQuality = 50;

        /// Combines a strain's Potency trait + baseline effect profile with
        /// up to 3 ingredients. See docs/systems-design.md "Ingredients & Mixing".
        public static MixResult Mix(Strain strain, IReadOnlyList<Ingredient> ingredients)
        {
            if (ingredients.Count > MaxIngredientSlots)
                throw new ArgumentException($"A batch can use at most {MaxIngredientSlots} ingredients.");

            int potency = strain.Potency;
            int relaxation = strain.BaselineRelaxation;
            int energy = strain.BaselineEnergy;
            int focus = strain.BaselineFocus;
            int quality = BaseQuality;
            float yieldMultiplier = 1f;

            foreach (Ingredient ingredient in ingredients)
            {
                potency += ingredient.PotencyDelta;
                relaxation += ingredient.RelaxationDelta;
                energy += ingredient.EnergyDelta;
                focus += ingredient.FocusDelta;
                quality += ingredient.QualityDelta;
                yieldMultiplier += ingredient.YieldPercentDelta;
            }

            return new MixResult
            {
                Profile = new EffectProfile
                {
                    Potency = Clamp(potency),
                    Relaxation = Clamp(relaxation),
                    Energy = Clamp(energy),
                    Focus = Clamp(focus),
                },
                Quality = Clamp(quality),
                YieldMultiplier = yieldMultiplier,
            };
        }

        private static int Clamp(int value) => value < 0 ? 0 : (value > 100 ? 100 : value);
    }
}
