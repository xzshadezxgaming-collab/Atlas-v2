using System.Collections.Generic;
using StrainEmpire.Core;
using StrainEmpire.Core.Economy;
using StrainEmpire.Core.Genetics;
using StrainEmpire.Core.Market;
using StrainEmpire.Core.Mixing;
using StrainEmpire.Core.Random;
using StrainEmpire.Core.Session;
using StrainEmpire.Gameplay.Save;
using StrainEmpire.Gameplay.Cloud;
using StrainEmpire.Gameplay.UI;
using UnityEngine;
using UnityEngine.UI;

namespace StrainEmpire.Gameplay
{
    /// Scene entry point: owns the GameSession, ticks it against real time,
    /// builds a minimal (programmer-art) runtime UI, and wires save/load +
    /// Steam leaderboard/achievement submission. This class and everything
    /// under UI/ has NOT been run in a Unity Editor/Player in this session
    /// (none was available) — see docs/unity-project-notes.md for exactly
    /// what has and hasn't been verified.
    public class GameManager : MonoBehaviour
    {
        // UI only renders the first N plots; buying beyond this is still
        // tracked correctly in GameSession, it just won't be visible until
        // the real UI (a scrollable list) replaces this placeholder.
        private const int MaxDisplayedPlots = 8;
        private const float SecondsPerSeason = 7 * 24 * 3600f;

        private GameSession _session;
        private ILeaderboardService _leaderboard;
        private IAchievementService _achievements;

        private Text _cashText;
        private Text _seasonText;
        private Text _empireValueText;
        private readonly List<Text> _plotTexts = new List<Text>();
        private readonly List<Button> _plotButtons = new List<Button>();
        private readonly List<Button> _ingredientButtons = new List<Button>();
        private readonly List<Ingredient> _selectedIngredients = new List<Ingredient>();

        private float _secondsSinceSeasonStart;

        // One-shot achievement gates. Steam's own SetAchievement call is
        // idempotent, but these avoid repeat local-fallback log spam and
        // make "first X" semantics explicit.
        private bool _firstHarvestUnlocked;
        private bool _firstBreedUnlocked;
        private bool _firstTier4Unlocked;

        private void Awake()
        {
            _leaderboard = LeaderboardServiceFactory.Create();
            _achievements = AchievementServiceFactory.Create();

            var rng = new SystemRandomSource();

            if (SaveSystem.HasSave())
            {
                (_session, float offlineHours) = SaveSystem.Load(rng, EconomyConfig.OfflineCapBaseHours);
                if (offlineHours > 0f)
                    _session.AdvanceTime(offlineHours);
            }
            else
            {
                _session = new GameSession(rng, SeasonArchetype.BalancedMarket);
                CreateStarterStrains();
            }

            BuildUI();
        }

        private void CreateStarterStrains()
        {
            // 3 starter strains per docs/systems-design.md MVP scope, each
            // leaning toward a different effect tag so market-matching (via
            // the ingredient mix) has real texture from the start.
            _session.CreateStarterStrain("Emberleaf", potency: 35, yield: 25, speed: 25, resilience: 20, relaxation: 10, energy: 15, focus: 10);
            _session.CreateStarterStrain("Mossglow", potency: 20, yield: 25, speed: 25, resilience: 20, relaxation: 30, energy: 10, focus: 15);
            _session.CreateStarterStrain("Sunspire", potency: 20, yield: 25, speed: 25, resilience: 20, relaxation: 10, energy: 30, focus: 10);
        }

        private void Update()
        {
            float deltaHours = Time.deltaTime / 3600f;
            _session.AdvanceTime(deltaHours);

            _secondsSinceSeasonStart += Time.deltaTime;
            if (_secondsSinceSeasonStart >= SecondsPerSeason)
            {
                _secondsSinceSeasonStart = 0f;
                RotateSeason();
            }

            RefreshUI();
        }

        private void RotateSeason()
        {
            var archetypes = (SeasonArchetype[])System.Enum.GetValues(typeof(SeasonArchetype));
            SeasonArchetype next = archetypes[UnityEngine.Random.Range(0, archetypes.Length)];
            _session.AdvanceSeason(next);

            int empireValue = Mathf.RoundToInt(_session.ComputeEmpireValue());
            _leaderboard.SubmitScore("season_empire_value", empireValue);
            _leaderboard.SubmitScore("alltime_empire_value", empireValue);
        }

        private void OnApplicationQuit() => SaveSystem.Save(_session);

        private void OnApplicationPause(bool paused)
        {
            if (paused) SaveSystem.Save(_session);
        }

        // ---- UI ----

        private void BuildUI()
        {
            Canvas canvas = RuntimeUIBuilder.CreateCanvas();
            Transform root = canvas.transform;

            _cashText = RuntimeUIBuilder.CreateText(root, "CashText", "", new Vector2(20, -20), new Vector2(400, 30));
            _seasonText = RuntimeUIBuilder.CreateText(root, "SeasonText", "", new Vector2(20, -55), new Vector2(400, 30));
            _empireValueText = RuntimeUIBuilder.CreateText(root, "EmpireValueText", "", new Vector2(20, -90), new Vector2(400, 30));

            RuntimeUIBuilder.CreateButton(root, "Buy Plot", new Vector2(20, -130), new Vector2(160, 40), OnBuyPlotClicked);
            RuntimeUIBuilder.CreateButton(root, "Breed First Two", new Vector2(190, -130), new Vector2(200, 40), OnBreedClicked);

            for (int i = 0; i < MaxDisplayedPlots; i++)
            {
                float y = -190 - i * 45;
                Text plotText = RuntimeUIBuilder.CreateText(root, $"PlotText_{i}", "", new Vector2(20, y), new Vector2(500, 30));
                int plotIndex = i;
                Button plotButton = RuntimeUIBuilder.CreateButton(root, "Action", new Vector2(540, y), new Vector2(150, 30), () => OnPlotActionClicked(plotIndex));
                _plotTexts.Add(plotText);
                _plotButtons.Add(plotButton);
            }

            float ingredientsY = -190 - MaxDisplayedPlots * 45 - 20;
            RuntimeUIBuilder.CreateText(root, "IngredientsLabel", $"Mix (up to {MixingSystem.MaxIngredientSlots}) — applies to the next sale:", new Vector2(20, ingredientsY), new Vector2(500, 25), fontSize: 16);

            for (int i = 0; i < IngredientDatabase.All.Count; i++)
            {
                Ingredient ingredient = IngredientDatabase.All[i];
                float x = 20 + (i % 5) * 145;
                float y = ingredientsY - 35 - (i / 5) * 40;
                Button button = RuntimeUIBuilder.CreateButton(root, ingredient.Name, new Vector2(x, y), new Vector2(140, 32), () => OnIngredientToggled(ingredient));
                _ingredientButtons.Add(button);
            }
        }

        private void RefreshUI()
        {
            _cashText.text = $"Cash: {_session.Cash:0}";
            _seasonText.text = $"Season: {_session.CurrentSeason}";
            _empireValueText.text = $"Empire Value: {_session.ComputeEmpireValue():0}";

            for (int i = 0; i < MaxDisplayedPlots; i++)
            {
                bool exists = i < _session.Plots.Count;
                _plotTexts[i].gameObject.SetActive(exists);
                _plotButtons[i].gameObject.SetActive(exists);
                if (!exists) continue;

                GrowPlot plot = _session.Plots[i];
                if (!plot.IsPlanted)
                {
                    _plotTexts[i].text = $"Plot {i}: empty";
                    _plotButtons[i].GetComponentInChildren<Text>().text = "Plant";
                }
                else if (!plot.IsMature)
                {
                    float pct = Mathf.Clamp01(plot.ElapsedHours / plot.PlantedStrain.GrowTimeHours) * 100f;
                    _plotTexts[i].text = $"Plot {i}: {plot.PlantedStrain.Name} growing ({pct:0}%)";
                    _plotButtons[i].GetComponentInChildren<Text>().text = "...";
                }
                else
                {
                    _plotTexts[i].text = $"Plot {i}: {plot.PlantedStrain.Name} ready!";
                    _plotButtons[i].GetComponentInChildren<Text>().text = "Sell";
                }
            }

            for (int i = 0; i < IngredientDatabase.All.Count; i++)
            {
                bool selected = _selectedIngredients.Contains(IngredientDatabase.All[i]);
                _ingredientButtons[i].GetComponent<Image>().color = selected
                    ? new Color(0.25f, 0.55f, 0.3f, 1f)
                    : new Color(0.2f, 0.2f, 0.25f, 1f);
            }
        }

        // ---- Actions ----

        private void OnBuyPlotClicked() => _session.BuyPlot();

        private void OnBreedClicked()
        {
            if (!_session.CanBreed || _session.StrainInventory.Count < 2) return;
            Strain a = _session.StrainInventory[0];
            Strain b = _session.StrainInventory[1];
            IReadOnlyList<Strain> seeds = _session.Breed(a, b, $"{a.Name}x{b.Name}");
            if (seeds == null) return;

            if (!_firstBreedUnlocked)
            {
                _firstBreedUnlocked = true;
                _achievements.Unlock("first_breeding");
            }
        }

        private void OnIngredientToggled(Ingredient ingredient)
        {
            if (_selectedIngredients.Contains(ingredient))
            {
                _selectedIngredients.Remove(ingredient);
                return;
            }

            if (_selectedIngredients.Count >= MixingSystem.MaxIngredientSlots) return; // full — deselect one first
            _selectedIngredients.Add(ingredient);
        }

        private void OnPlotActionClicked(int plotIndex)
        {
            if (plotIndex >= _session.Plots.Count) return;
            GrowPlot plot = _session.Plots[plotIndex];

            if (!plot.IsPlanted)
            {
                if (_session.StrainInventory.Count == 0) return;
                _session.Plant(plotIndex, _session.StrainInventory[0]);
            }
            else if (plot.IsMature)
            {
                Strain harvestedTraits = plot.PlantedStrain; // read before HarvestMixAndSell clears the plot
                float revenue = _session.HarvestMixAndSell(plotIndex, _selectedIngredients);
                if (revenue <= 0f) return;

                if (!_firstHarvestUnlocked)
                {
                    _firstHarvestUnlocked = true;
                    _achievements.Unlock("first_harvest");
                }

                if (!_firstTier4Unlocked && harvestedTraits.GeneticsTier >= 4)
                {
                    _firstTier4Unlocked = true;
                    _achievements.Unlock("first_tier4_strain");
                }
            }
        }
    }
}
