using System;
using System.Collections.Generic;

namespace StrainEmpire.Gameplay.Save
{
    [Serializable]
    public class StrainData
    {
        public string id;
        public string name;
        public int potency;
        public int yield;
        public int speed;
        public int resilience;
        public int baselineRelaxation;
        public int baselineEnergy;
        public int baselineFocus;
    }

    [Serializable]
    public class PlotData
    {
        public bool isPlanted;
        public float elapsedHours;
        public StrainData plantedStrain; // null (unset) when isPlanted is false
    }

    [Serializable]
    public class SaveData
    {
        public float cash;
        public List<PlotData> plots = new List<PlotData>();
        public List<StrainData> strainInventory = new List<StrainData>();
        public string currentSeason; // SeasonArchetype enum name
        public int nextStrainId;

        /// ISO 8601 UTC timestamp, used to compute offline catch-up time on next load.
        public string lastSavedUtc;
    }
}
