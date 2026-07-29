using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using StrainEmpire.Core;
using StrainEmpire.Core.Economy;
using StrainEmpire.Core.Genetics;
using StrainEmpire.Core.Market;
using StrainEmpire.Core.Random;
using StrainEmpire.Core.Session;
using UnityEngine;

namespace StrainEmpire.Gameplay.Save
{
    /// Local JSON save (UnityEngine.JsonUtility) under Application.persistentDataPath.
    /// If Steam Cloud is configured for that folder in the Steamworks partner
    /// site (App Admin > Cloud), this file syncs automatically with no extra
    /// code — see docs/steam-publishing-checklist.md. Converts to/from the
    /// Core GameSession via GameSession.Restore / GrowPlot.Restore, so the
    /// actual game-state logic stays in the tested, UnityEngine-independent
    /// Core layer.
    public static class SaveSystem
    {
        private const string FileName = "savegame.json";

        private static string FilePath => Path.Combine(Application.persistentDataPath, FileName);

        public static bool HasSave() => File.Exists(FilePath);

        public static void Save(GameSession session)
        {
            var data = new SaveData
            {
                cash = session.Cash,
                currentSeason = session.CurrentSeason.ToString(),
                nextStrainId = GetNextStrainId(session),
                lastSavedUtc = DateTime.UtcNow.ToString("o"),
            };

            foreach (GrowPlot plot in session.Plots)
            {
                data.plots.Add(new PlotData
                {
                    isPlanted = plot.IsPlanted,
                    elapsedHours = plot.ElapsedHours,
                    plantedStrain = plot.IsPlanted ? ToStrainData(plot.PlantedStrain) : null,
                });
            }

            foreach (Strain strain in session.StrainInventory)
                data.strainInventory.Add(ToStrainData(strain));

            string json = JsonUtility.ToJson(data, prettyPrint: true);
            File.WriteAllText(FilePath, json);
        }

        /// Returns the restored session and the real-time hours elapsed
        /// since the save (already capped by the offline cap) so the caller
        /// can immediately AdvanceTime() with it.
        public static (GameSession session, float offlineCatchUpHours) Load(IRandomSource rng, int offlineCapHours)
        {
            string json = File.ReadAllText(FilePath);
            SaveData data = JsonUtility.FromJson<SaveData>(json);

            var plots = new List<GrowPlot>();
            foreach (PlotData plotData in data.plots)
            {
                Strain strain = plotData.isPlanted ? FromStrainData(plotData.plantedStrain) : null;
                plots.Add(GrowPlot.Restore(strain, plotData.elapsedHours));
            }

            List<Strain> inventory = data.strainInventory.Select(FromStrainData).ToList();
            var season = (SeasonArchetype)Enum.Parse(typeof(SeasonArchetype), data.currentSeason);

            GameSession session = GameSession.Restore(rng, data.cash, plots, inventory, season, data.nextStrainId);

            double elapsedRealSeconds = 0;
            if (DateTime.TryParse(data.lastSavedUtc, null, System.Globalization.DateTimeStyles.RoundtripKind, out DateTime lastSaved))
                elapsedRealSeconds = (DateTime.UtcNow - lastSaved).TotalSeconds;

            float catchUpHours = OfflineProgressCalculator.ComputeCatchUpHours(elapsedRealSeconds, offlineCapHours);
            return (session, catchUpHours);
        }

        private static int GetNextStrainId(GameSession session)
        {
            // Strain IDs are "strain-N"; scan the highest N in play so a
            // freshly restored session keeps handing out unique IDs.
            IEnumerable<Strain> all = session.StrainInventory
                .Concat(session.Plots.Where(p => p.IsPlanted).Select(p => p.PlantedStrain));

            int maxId = 0;
            foreach (Strain strain in all)
            {
                string[] parts = strain.Id.Split('-');
                if (parts.Length == 2 && int.TryParse(parts[1], out int n) && n > maxId)
                    maxId = n;
            }
            return maxId + 1;
        }

        private static StrainData ToStrainData(Strain strain) => new StrainData
        {
            id = strain.Id,
            name = strain.Name,
            potency = strain.Potency,
            yield = strain.Yield,
            speed = strain.Speed,
            resilience = strain.Resilience,
            baselineRelaxation = strain.BaselineRelaxation,
            baselineEnergy = strain.BaselineEnergy,
            baselineFocus = strain.BaselineFocus,
        };

        private static Strain FromStrainData(StrainData data) => new Strain
        {
            Id = data.id,
            Name = data.name,
            Potency = data.potency,
            Yield = data.yield,
            Speed = data.speed,
            Resilience = data.resilience,
            BaselineRelaxation = data.baselineRelaxation,
            BaselineEnergy = data.baselineEnergy,
            BaselineFocus = data.baselineFocus,
        };
    }
}
