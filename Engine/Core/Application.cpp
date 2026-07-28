#include "Application.h"

#include "../Actors/Actor.h"
#include "../Actors/AIController.h"
#include "../Audio/Audio.h"
#include "BuyCart.h"
#include "IniFile.h"
#include "../Combat/Delivery.h"
#include "../Combat/Grenade.h"
#include "../Combat/Weapon.h"
#include "../Graphics/Background.h"
#include "../Graphics/Camera.h"
#include "../Graphics/PixelFont.h"
#include "../Input/Input.h"
#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace Atlas
{
    namespace
    {
        constexpr int ViewWidth = 1280;
        constexpr int ViewHeight = 720;

        constexpr int DefaultWorldWidth = 2048;
        constexpr int DefaultWorldHeight = 1024;

        // Physics runs at a fixed rate; rendering runs as fast as it can.
        constexpr float FixedTimeStep = 1.0f / 120.0f;
        constexpr float MaxFrameTime = 0.25f;

        constexpr float GrenadeCooldownTime = 0.9f;
        constexpr float RespawnTime = 3.0f;
        constexpr int MaxEnemies = 8;
        constexpr int MaxAllies = 4;

        constexpr int SupplyCrateCost = 40;
        constexpr int ReinforcementCost = 100;

        // Bots-spawn toggle button (screen space, top right).
        constexpr float BotsButtonW = 108.0f;
        constexpr float BotsButtonH = 20.0f;
        constexpr float BotsButtonX =
            static_cast<float>(ViewWidth) - 20.0f - BotsButtonW;
        constexpr float BotsButtonY = 54.0f;

        // Floating buy menu (world space, anchored above the player).
        constexpr float WheelButtonW = 56.0f;
        constexpr float WheelButtonH = 36.0f;
        constexpr float WheelGap = 8.0f;
        constexpr float WheelTotalW =
            WheelButtonW * 3.0f + WheelGap * 2.0f;

        constexpr float BuyPanelW = 260.0f;
        constexpr float BuyPanelH = 328.0f;
        constexpr float CloseButtonSize = 18.0f;

        // Order-panel catalog row layout.
        constexpr float CatalogRowH = 15.0f;
        constexpr float CatalogRowGap = 2.0f;
        constexpr float CatalogGroupGap = 5.0f; // extra gap between BODIES/WEAPONS/SUPPLIES

        std::string ResolveAssetPath(const std::string& relative)
        {
            const char* basePath = SDL_GetBasePath();

            const std::string candidates[] =
            {
                relative,
                "../" + relative,
                "../../" + relative,
                "../../../../" + relative,
                basePath ? std::string(basePath) + relative : std::string(),
            };

            for (const std::string& path : candidates)
            {
                if (path.empty())
                    continue;

                SDL_IOStream* file = SDL_IOFromFile(path.c_str(), "rb");

                if (file)
                {
                    SDL_CloseIO(file);
                    return path;
                }
            }

            return relative;
        }

        const WeaponDef* FindWeaponDef(
            const std::vector<WeaponDef>& defs,
            const std::string& name)
        {
            for (const WeaponDef& def : defs)
            {
                if (def.Name == name)
                    return &def;
            }

            return defs.empty() ? nullptr : &defs.front();
        }

        Sfx FireSfxFor(const WeaponDef& def)
        {
            if (def.Kind == WeaponKind::Digger ||
                def.Kind == WeaponKind::Shovel)
                return Sfx::Dig;

            if (def.Name == "Shotgun")
                return Sfx::Shotgun;

            if (def.Name == "Rifle")
                return Sfx::Rifle;

            return Sfx::Shot;
        }

        struct Enemy
        {
            std::unique_ptr<Actor> Body;
            AIController Brain;

            // Health seen last tick, for spawning floating damage numbers
            // without plumbing callbacks through the damage paths.
            int LastHealth = 100;
        };

        // A short-lived piece of floating text: damage numbers, gold
        // popups, wave banners. World-space entries scroll with the
        // camera; screen-space entries (banners) don't.
        struct Floater
        {
            float X;
            float Y;
            float VelY;
            float Life;
            float MaxLife;
            float Scale;
            std::string Text;
            Uint8 R, G, B;
            bool ScreenSpace;
        };

        // A purchased reinforcement, once landed: friendly, AI-controlled,
        // fights whatever enemy is nearest.
        struct Ally
        {
            std::unique_ptr<Actor> Body;
            AIController Brain;
        };

        struct HealthPickup
        {
            float X;
            float Y;
            float VelY;
        };

        // --- Buy-menu catalog: what's on offer, Cortex Command style ---
        enum class CatalogKind
        {
            Body,
            Weapon,
            Item,
        };

        struct CatalogEntry
        {
            CatalogKind Kind;
            std::string Name;
            int Cost;
            const WeaponDef* Weapon; // Body: default loadout; Weapon: itself; Item: null
        };

        // One line in the cart summary display: a purchased body (with
        // its currently-assigned weapon), a group of identical loose
        // weapons, or the supply-crate count.
        struct CartLine
        {
            std::string Text;
            int Kind; // 0 = body, 1 = loose weapon group, 2 = supply crates
            int RefIndex; // body index, or the loose weapon's first index
        };

        // A weapon crate ordered without an assigned body: sits on the
        // ground where it landed until the player walks over it, then
        // equips it (fully loaded) as their current weapon.
        struct WeaponPickup
        {
            float X;
            float Y;
            float VelY;
            const WeaponDef* Weapon;
        };

        // Off-screen margin the delivery ship starts beyond the camera's
        // current edge, so it visibly flies in rather than popping into
        // view already on screen.
        constexpr float ShipEdgeMargin = 220.0f;
    }

    Application::Application()
    {
    }

    void Application::Run()
    {
        std::cout << "=================================\n";
        std::cout << "        ATLAS ENGINE\n";
        std::cout << "           v0.5.0\n";
        std::cout << "=================================\n\n";

        if (!m_Window.Create("Atlas", ViewWidth, ViewHeight))
        {
            return;
        }

        if (!Audio::Init())
        {
            std::cout << "(no audio device - running silent)\n";
        }

        // The game draws its own crosshair glued to the mouse position;
        // the OS cursor on top of it would just be a second pointer.
        SDL_HideCursor();

        Camera camera;
        m_Window.SetCamera(&camera);

        // Scene setup is data-driven; Seed 0 means a fresh world each run.
        IniFile sceneIni;
        sceneIni.Load(ResolveAssetPath("Assets/scene.ini"));

        const int worldWidth = std::max(
            ViewWidth,
            sceneIni.GetInt("World", "Width", DefaultWorldWidth));
        const int worldHeight = std::max(
            ViewHeight,
            sceneIni.GetInt("World", "Height", DefaultWorldHeight));

        unsigned int seed = static_cast<unsigned int>(
            sceneIni.GetInt("World", "Seed", 0));

        if (seed == 0)
            seed = static_cast<unsigned int>(SDL_GetPerformanceCounter());

        Terrain terrain;

        if (!terrain.Create(
            m_Window.GetRenderer(),
            worldWidth,
            worldHeight,
            seed))
        {
            return;
        }

        Background background;
        background.Create(seed);

        // Weapons: data-driven with built-in fallbacks.
        const std::vector<WeaponDef> weaponDefs =
            LoadWeaponDefs(ResolveAssetPath("Assets/weapons.ini"));

        const WeaponDef* loadout[] =
        {
            FindWeaponDef(weaponDefs, "SMG"),
            FindWeaponDef(weaponDefs, "Shotgun"),
            FindWeaponDef(weaponDefs, "Rifle"),
            FindWeaponDef(weaponDefs, "Digger"),
            FindWeaponDef(weaponDefs, "Shovel"),
        };

        constexpr int LoadoutSize = 5;

        const WeaponDef* enemyWeapon = FindWeaponDef(weaponDefs, "EnemyGun");

        const std::string playerSprite = ResolveAssetPath("Assets/Player.bmp");

        Actor player;

        if (!player.LoadBodySprite(m_Window.GetRenderer(), playerSprite))
        {
            std::cout << "Failed to load Player.bmp\n";
        }

        player.SetTeam(0);
        player.SetWeaponDef(loadout[0]);
        player.Spawn(terrain, static_cast<float>(worldWidth) * 0.5f);

        int currentWeapon = 0;

        ParticleSystem particles;
        GrenadeSystem grenades;

        std::vector<Enemy> enemies;
        std::vector<Ally> allies;
        DeliverySystem deliveries;

        const WeaponDef* allyWeapon = FindWeaponDef(weaponDefs, "SMG");

        // weapon: null falls back to the default rifleman loadout (SMG).
        auto spawnAlly = [&](float x, const WeaponDef* weapon)
        {
            if (static_cast<int>(allies.size()) >= MaxAllies)
                return;

            Ally ally;
            ally.Body = std::make_unique<Actor>();
            ally.Body->LoadBodySprite(m_Window.GetRenderer(), playerSprite);
            ally.Body->SetTint(120, 170, 255);
            ally.Body->SetTeam(0);
            ally.Body->SetWeaponDef(weapon ? weapon : allyWeapon);
            ally.Body->Spawn(terrain, x);
            allies.push_back(std::move(ally));
        };

        std::vector<WeaponPickup> weaponPickups;
        BuyCart cart;

        // Catalog is fixed for the whole run (built from the same
        // WeaponDefs the player's own loadout uses), so it - and the
        // vertical offset of each row within the panel - are computed
        // once rather than every frame.
        std::vector<CatalogEntry> catalog;
        catalog.push_back(
            { CatalogKind::Body, "RIFLEMAN", ReinforcementCost, allyWeapon });

        for (int i = 0; i < LoadoutSize; i++)
        {
            std::string name = loadout[i]->Name;

            for (char& c : name)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

            catalog.push_back(
                { CatalogKind::Weapon, name, loadout[i]->Cost, loadout[i] });
        }

        catalog.push_back(
            { CatalogKind::Item, "SUPPLY CRATE", SupplyCrateCost, nullptr });

        std::vector<float> catalogRowOffset(catalog.size());
        constexpr float CatalogStartY = 46.0f;

        {
            float y = CatalogStartY;

            for (std::size_t i = 0; i < catalog.size(); i++)
            {
                catalogRowOffset[i] = y;
                y += CatalogRowH + CatalogRowGap;

                if (i + 1 < catalog.size() &&
                    catalog[i].Kind != catalog[i + 1].Kind)
                {
                    y += CatalogGroupGap;
                }
            }
        }

        // Layout for everything below the catalog: the assignment
        // toggle, the cart summary lines, the total, and the two
        // action buttons. Computed once (from the fixed catalog height)
        // so click-handling and drawing always agree on where things are.
        constexpr float CartLineH = 11.0f;
        constexpr int MaxCartLinesShown = 6;
        constexpr float SendButtonH = 22.0f;
        constexpr float ClearButtonH = 16.0f;

        const float assignRowOffset =
            catalogRowOffset.back() + CatalogRowH + 6.0f;
        const float cartLinesStartOffset = assignRowOffset + 14.0f;
        const float totalLineOffset =
            cartLinesStartOffset + MaxCartLinesShown * CartLineH + 6.0f;
        const float sendButtonOffset = totalLineOffset + 14.0f;
        const float clearButtonOffset =
            sendButtonOffset + SendButtonH + 6.0f;

        // Builds the (up to) one-line-per-order-item cart summary,
        // grouping identical loose weapons together. Recomputed on
        // demand rather than cached, since the cart mutates on clicks.
        auto buildCartLines = [&]()
        {
            std::vector<CartLine> lines;

            const std::vector<CartBody>& bodies = cart.GetBodies();

            for (std::size_t i = 0; i < bodies.size(); i++)
            {
                std::string weaponName =
                    bodies[i].Weapon ? bodies[i].Weapon->Name : "UNARMED";

                for (char& c : weaponName)
                    c = static_cast<char>(
                        std::toupper(static_cast<unsigned char>(c)));

                lines.push_back(
                    { "RIFLEMAN + " + weaponName, 0, static_cast<int>(i) });
            }

            const std::vector<CartLooseWeapon>& loose = cart.GetLooseWeapons();
            std::vector<bool> consumed(loose.size(), false);

            for (std::size_t i = 0; i < loose.size(); i++)
            {
                if (consumed[i])
                    continue;

                int count = 1;

                for (std::size_t j = i + 1; j < loose.size(); j++)
                {
                    if (!consumed[j] && loose[j].Weapon == loose[i].Weapon)
                    {
                        consumed[j] = true;
                        count++;
                    }
                }

                std::string weaponName =
                    loose[i].Weapon ? loose[i].Weapon->Name : "?";

                for (char& c : weaponName)
                    c = static_cast<char>(
                        std::toupper(static_cast<unsigned char>(c)));

                const std::string text =
                    (count > 1 ? std::to_string(count) + "x " : std::string()) +
                    weaponName + " (LOOSE)";

                lines.push_back({ text, 1, static_cast<int>(i) });
            }

            if (cart.GetSupplyCrateCount() > 0)
            {
                const std::string text =
                    (cart.GetSupplyCrateCount() > 1
                        ? std::to_string(cart.GetSupplyCrateCount()) + "x "
                        : std::string()) +
                    "SUPPLY CRATE";

                lines.push_back({ text, 2, 0 });
            }

            return lines;
        };

        std::mt19937 rng(
            static_cast<unsigned int>(SDL_GetPerformanceCounter()));

        int wave = 0;
        float waveTimer = 2.5f;
        float grenadeCooldown = 0.0f;
        float respawnTimer = 0.0f;
        float jetSoundTimer = 0.0f;
        float toolSfxTimer = 0.0f;
        bool playerGibbed = false;
        bool playerWasGrounded = true;
        float playerPrevFallSpeed = 0.0f;

        // Camera: exponentially-smoothed position with aim lookahead and
        // trauma-based shake (trauma in [0,1]; amplitude scales with its
        // square so small hits barely tremble and big ones really rock).
        float camSmoothX = player.GetCenterX() - ViewWidth * 0.5f;
        float camSmoothY = player.GetCenterY() - ViewHeight * 0.5f;
        float trauma = 0.0f;

        std::vector<HealthPickup> pickups;
        std::vector<Floater> floaters;
        std::uniform_real_distribution<float> unit(-1.0f, 1.0f);

        // Gold gained is batched into one popup instead of spamming a
        // floater for every mined pixel.
        int goldPopupAccum = 0;
        float goldPopupTimer = 0.0f;
        int lastPlayerGold = player.GetGold();

        // Footstep dust needs plant-edge detection per leg.
        bool prevLegPlanted[2] = { true, true };

        // Crosshair blooms open briefly on each shot.
        float crosshairBloom = 0.0f;

        // A few frames of frozen simulation on each kill (hit-stop) -
        // rendering continues, so the freeze reads as impact, not lag.
        float hitStop = 0.0f;

        // Edge-triggered inputs are latched until a sim tick actually
        // runs: hit-stop can starve the sim for a few render frames, and
        // a per-render-frame edge landing in one of them would otherwise
        // be silently dropped.
        bool throwQueued = false;
        bool jumpQueued = false;

        // The UI (buy menu, HUD anchors) follows the smoothed camera
        // WITHOUT the shake component, so menus don't wobble and clicks
        // can't land on a shifted row mid-shake. Updated each frame from
        // the camera block below.
        float uiCamX = camSmoothX;
        float uiCamY = camSmoothY;

        // Health bar keeps a decaying "ghost" of recent damage.
        float healthGhost = 1.0f;

        float timeSeconds = 0.0f;
        float hurtVignette = 0.0f;
        int lastPlayerHealth = player.GetHealth();

        // Bots-spawn toggle and the Tab-held buy menu.
        bool botsEnabled = true;
        bool buyPanelOpen = false;

        auto spawnEnemy = [&](float x)
        {
            Enemy enemy;
            enemy.Body = std::make_unique<Actor>();
            enemy.Body->LoadBodySprite(m_Window.GetRenderer(), playerSprite);
            enemy.Body->SetTint(255, 118, 106);
            enemy.Body->SetTeam(1);
            enemy.Body->SetWeaponDef(enemyWeapon);
            enemy.Body->Spawn(terrain, x);
            enemies.push_back(std::move(enemy));
        };

        auto spawnWave = [&](int count)
        {
            std::uniform_real_distribution<float> position(
                150.0f, static_cast<float>(worldWidth - 150));

            for (int i = 0; i < count && static_cast<int>(enemies.size()) < MaxEnemies; i++)
            {
                float x = position(rng);

                for (int attempt = 0;
                    attempt < 10 &&
                    std::fabs(x - player.GetCenterX()) < 380.0f;
                    attempt++)
                {
                    x = position(rng);
                }

                spawnEnemy(x);
            }
        };

        bool running = true;

        Uint64 lastCounter = SDL_GetPerformanceCounter();
        const Uint64 frequency = SDL_GetPerformanceFrequency();

        float accumulator = 0.0f;

        double fpsTimer = 0.0;
        int frames = 0;

        while (running)
        {
            const Uint64 currentCounter = SDL_GetPerformanceCounter();

            float frameTime = static_cast<float>(
                static_cast<double>(currentCounter - lastCounter) /
                static_cast<double>(frequency));

            lastCounter = currentCounter;

            frameTime = std::min(frameTime, MaxFrameTime);

            timeSeconds += frameTime;
            fpsTimer += frameTime;
            frames++;

            if (fpsTimer >= 1.0)
            {
                std::stringstream title;
                title << "Atlas - " << frames << " FPS - wave " << wave
                    << " - " << particles.GetActiveCount() << " particles";
                m_Window.SetTitle(title.str());

                fpsTimer = 0.0;
                frames = 0;
            }

            running = m_Window.PollEvents();

            Input::Update();

            // Weapon switching (1-5 or scroll wheel) and reload are
            // edge-triggered per frame.
            if (player.IsAlive())
            {
                if (Input::WasKeyPressed(SDL_SCANCODE_1)) currentWeapon = 0;
                if (Input::WasKeyPressed(SDL_SCANCODE_2)) currentWeapon = 1;
                if (Input::WasKeyPressed(SDL_SCANCODE_3)) currentWeapon = 2;
                if (Input::WasKeyPressed(SDL_SCANCODE_4)) currentWeapon = 3;
                if (Input::WasKeyPressed(SDL_SCANCODE_5)) currentWeapon = 4;

                const int wheel = Input::ConsumeWheelSteps();

                if (wheel != 0)
                {
                    currentWeapon =
                        (currentWeapon - wheel % LoadoutSize +
                            LoadoutSize) % LoadoutSize;
                }

                if (player.GetWeapon().GetDef() != loadout[currentWeapon])
                {
                    player.SetWeaponDef(loadout[currentWeapon]);
                    Audio::Play(Sfx::Reload, 0.7f);
                }

                if (Input::WasKeyPressed(SDL_SCANCODE_R))
                {
                    player.GetWeapon().StartReload();
                    Audio::Play(Sfx::Reload);
                }
            }

            const float mouseWorldX = Input::GetMouseX() + camera.GetX();
            const float mouseWorldY = Input::GetMouseY() + camera.GetY();

            throwQueued = throwQueued ||
                Input::WasMouseButtonPressed(SDL_BUTTON_RIGHT);
            jumpQueued = jumpQueued ||
                Input::WasKeyPressed(SDL_SCANCODE_SPACE);

            // --- UI: bots-spawn toggle button, and the Tab buy menu ---
            //
            // Click rects are computed once here (before this frame's
            // simulation step) and the exact same numbers are reused for
            // drawing later, via DrawScreenRect, so what gets clicked and
            // what gets drawn can never disagree.
            const float mouseScreenX = Input::GetMouseX();
            const float mouseScreenY = Input::GetMouseY();

            const bool botsButtonHovered =
                mouseScreenX >= BotsButtonX &&
                mouseScreenX <= BotsButtonX + BotsButtonW &&
                mouseScreenY >= BotsButtonY &&
                mouseScreenY <= BotsButtonY + BotsButtonH;

            if (Input::WasMouseButtonPressed(SDL_BUTTON_LEFT) &&
                botsButtonHovered)
            {
                botsEnabled = !botsEnabled;
            }

            const bool menuHeld =
                player.IsAlive() && Input::IsKeyDown(SDL_SCANCODE_TAB);

            if (!menuHeld)
                buyPanelOpen = false;

            const float uiCameraX = uiCamX;
            const float uiCameraY = uiCamY;

            // Floating menu anchor, in world space, above the player.
            const float menuAnchorX = player.GetCenterX();
            const float menuAnchorY = player.GetY() - 70.0f;

            const float wheelScreenX =
                menuAnchorX - WheelTotalW * 0.5f - uiCameraX;
            const float wheelScreenY =
                menuAnchorY - WheelButtonH * 0.5f - uiCameraY;

            const float panelScreenX =
                menuAnchorX - BuyPanelW * 0.5f - uiCameraX;
            const float panelScreenY =
                menuAnchorY - BuyPanelH * 0.5f - uiCameraY;

            const float closeScreenX =
                panelScreenX + BuyPanelW - CloseButtonSize - 6.0f;
            const float closeScreenY = panelScreenY + 6.0f;

            if (menuHeld && Input::WasMouseButtonPressed(SDL_BUTTON_LEFT))
            {
                if (!buyPanelOpen)
                {
                    const float buyX = wheelScreenX;
                    const float buyY = wheelScreenY;

                    if (mouseScreenX >= buyX &&
                        mouseScreenX <= buyX + WheelButtonW &&
                        mouseScreenY >= buyY &&
                        mouseScreenY <= buyY + WheelButtonH)
                    {
                        buyPanelOpen = true;
                    }
                }
                else if (mouseScreenX >= closeScreenX &&
                    mouseScreenX <= closeScreenX + CloseButtonSize &&
                    mouseScreenY >= closeScreenY &&
                    mouseScreenY <= closeScreenY + CloseButtonSize)
                {
                    buyPanelOpen = false;
                }
                else
                {
                    const float rowX = panelScreenX + 8.0f;
                    const float rowW = BuyPanelW - 16.0f;

                    const float assignY = panelScreenY + assignRowOffset;
                    const float sendY = panelScreenY + sendButtonOffset;
                    const float clearY = panelScreenY + clearButtonOffset;

                    bool handled = false;

                    // Catalog rows: click to add to the cart.
                    for (std::size_t i = 0; i < catalog.size() && !handled; i++)
                    {
                        const float rowY = panelScreenY + catalogRowOffset[i];

                        if (mouseScreenX < rowX ||
                            mouseScreenX > rowX + rowW ||
                            mouseScreenY < rowY ||
                            mouseScreenY > rowY + CatalogRowH)
                            continue;

                        handled = true;

                        const CatalogEntry& entry = catalog[i];

                        if (player.GetGold() < cart.GetTotalCost() + entry.Cost)
                            break; // can't afford - clicking does nothing

                        if (entry.Kind == CatalogKind::Body &&
                            static_cast<int>(cart.GetBodies().size()) +
                                    static_cast<int>(allies.size()) >=
                                MaxAllies)
                        {
                            break; // no room to field another body
                        }

                        switch (entry.Kind)
                        {
                        case CatalogKind::Body:
                            cart.AddBody(entry.Weapon, entry.Cost);
                            break;
                        case CatalogKind::Weapon:
                            cart.AddWeapon(entry.Weapon, entry.Cost);
                            break;
                        case CatalogKind::Item:
                            cart.AddSupplyCrate(entry.Cost);
                            break;
                        }

                        Audio::Play(Sfx::Reload, 0.35f);
                    }

                    // Assignment toggle: cycles which cart body (if any)
                    // the next purchased weapon equips onto, or LOOSE.
                    if (!handled &&
                        mouseScreenX >= rowX && mouseScreenX <= rowX + rowW &&
                        mouseScreenY >= assignY &&
                        mouseScreenY <= assignY + CatalogRowH)
                    {
                        handled = true;

                        const int bodyCount =
                            static_cast<int>(cart.GetBodies().size());
                        const int next = cart.GetActiveBodyIndex() + 1;

                        if (bodyCount == 0 || next >= bodyCount)
                            cart.DeselectBody();
                        else
                            cart.SelectBody(next);
                    }

                    // Cart lines: click a body line to make it the active
                    // assignment target again.
                    if (!handled)
                    {
                        const std::vector<CartLine> lines = buildCartLines();

                        for (std::size_t i = 0;
                            i < lines.size() &&
                                i < static_cast<std::size_t>(MaxCartLinesShown) &&
                                !handled;
                            i++)
                        {
                            const float lineY = panelScreenY +
                                cartLinesStartOffset +
                                static_cast<float>(i) * CartLineH;

                            if (mouseScreenX < rowX ||
                                mouseScreenX > rowX + rowW ||
                                mouseScreenY < lineY ||
                                mouseScreenY > lineY + CartLineH)
                                continue;

                            handled = true;

                            if (lines[i].Kind == 0)
                                cart.SelectBody(lines[i].RefIndex);
                        }
                    }

                    // SEND ORDER: spend the total, queue one delivery per
                    // body, per loose weapon, and per supply crate.
                    if (!handled &&
                        mouseScreenX >= rowX && mouseScreenX <= rowX + rowW &&
                        mouseScreenY >= sendY &&
                        mouseScreenY <= sendY + SendButtonH)
                    {
                        handled = true;

                        if (!cart.IsEmpty() &&
                            player.GetGold() >= cart.GetTotalCost())
                        {
                            player.AddGold(-cart.GetTotalCost());

                            auto queueDelivery = [&](
                                DeliveryKind kind, const WeaponDef* weapon)
                            {
                                const bool fromLeft = (rng() % 2u) == 0u;
                                const float shipDir = fromLeft ? 1.0f : -1.0f;
                                const float shipOrigin = fromLeft
                                    ? (camera.GetX() - ShipEdgeMargin)
                                    : (camera.GetX() +
                                        static_cast<float>(ViewWidth) +
                                        ShipEdgeMargin);

                                deliveries.Order(
                                    kind, shipOrigin, shipDir,
                                    player.GetCenterX(), weapon);
                            };

                            for (const CartBody& body : cart.GetBodies())
                            {
                                queueDelivery(
                                    DeliveryKind::Reinforcement, body.Weapon);
                            }

                            for (const CartLooseWeapon& loose :
                                cart.GetLooseWeapons())
                            {
                                queueDelivery(
                                    DeliveryKind::WeaponPickup, loose.Weapon);
                            }

                            for (int i = 0;
                                i < cart.GetSupplyCrateCount(); i++)
                            {
                                queueDelivery(
                                    DeliveryKind::SupplyCrate, nullptr);
                            }

                            cart.Clear();
                            Audio::Play(Sfx::Reload, 0.6f);
                        }
                    }

                    // CLEAR: abandon the whole order (nothing was spent
                    // yet - gold is only taken on SEND).
                    if (!handled &&
                        mouseScreenX >= rowX && mouseScreenX <= rowX + rowW &&
                        mouseScreenY >= clearY &&
                        mouseScreenY <= clearY + ClearButtonH)
                    {
                        cart.Clear();
                    }
                }
            }

            // Right-click a cart line to cancel just that part of the
            // order (refunded automatically - nothing was spent yet).
            if (menuHeld && buyPanelOpen &&
                Input::WasMouseButtonPressed(SDL_BUTTON_RIGHT))
            {
                const float rowX = panelScreenX + 8.0f;
                const float rowW = BuyPanelW - 16.0f;

                const std::vector<CartLine> lines = buildCartLines();

                for (std::size_t i = 0;
                    i < lines.size() &&
                        i < static_cast<std::size_t>(MaxCartLinesShown);
                    i++)
                {
                    const float lineY = panelScreenY +
                        cartLinesStartOffset +
                        static_cast<float>(i) * CartLineH;

                    if (mouseScreenX < rowX || mouseScreenX > rowX + rowW ||
                        mouseScreenY < lineY ||
                        mouseScreenY > lineY + CartLineH)
                        continue;

                    if (lines[i].Kind == 0)
                        cart.RemoveBody(lines[i].RefIndex);
                    else if (lines[i].Kind == 1)
                        cart.RemoveLooseWeapon(lines[i].RefIndex);
                    else
                        cart.RemoveSupplyCrate();

                    break;
                }
            }

            // While the buy menu is open, or the mouse is over a HUD
            // button, the world shouldn't respond to left/right clicks
            // (no shooting or grenade-throwing through the UI). Movement
            // is left untouched so the player can still walk around with
            // the menu open.
            const bool uiCapturingInput = menuHeld || botsButtonHovered;

            // Digger plasma-beam preview: a raycast every rendered frame
            // (not tied to the tool's own slower fire-rate ticks) so the
            // beam tracks the aim smoothly while held down.
            if (player.IsAlive())
            {
                const Weapon& heldWeapon = player.GetWeapon();

                const bool isDigging =
                    heldWeapon.GetDef() &&
                    heldWeapon.GetDef()->Kind == WeaponKind::Digger &&
                    Input::IsMouseButtonDown(SDL_BUTTON_LEFT) &&
                    !uiCapturingInput &&
                    !player.IsArmDestroyed();

                if (isDigging)
                {
                    const WeaponDef& def = *heldWeapon.GetDef();
                    const float muzzleX = player.GetMuzzleX();
                    const float muzzleY = player.GetMuzzleY();

                    float dirX = player.GetAimDirX();
                    float dirY = player.GetAimDirY();
                    heldWeapon.GetSweptDirection(
                        player.GetAimDirX(), player.GetAimDirY(), dirX, dirY);

                    float hitX = 0.0f;
                    float hitY = 0.0f;

                    if (terrain.RaycastSolid(
                        muzzleX, muzzleY, dirX, dirY,
                        def.DigRange, hitX, hitY))
                    {
                        player.SetDigBeam(true, hitX, hitY);
                    }
                    else
                    {
                        player.SetDigBeam(
                            true,
                            muzzleX + dirX * def.DigRange,
                            muzzleY + dirY * def.DigRange);
                    }
                }
                else
                {
                    player.SetDigBeam(false, 0.0f, 0.0f);
                }
            }

            // Fixed-timestep simulation. Hit-stop eats the frame's sim
            // time instead of feeding the accumulator, freezing the
            // world for a beat while rendering carries on.
            if (hitStop > 0.0f)
                hitStop -= frameTime;
            else
                accumulator += frameTime;

            bool simTicked = false;

            while (accumulator >= FixedTimeStep)
            {
                grenadeCooldown -= FixedTimeStep;
                jetSoundTimer -= FixedTimeStep;
                toolSfxTimer -= FixedTimeStep;

                // --- Player ---
                if (player.IsAlive())
                {
                    float moveInput = 0.0f;

                    if (Input::IsKeyDown(SDL_SCANCODE_A))
                        moveInput -= 1.0f;

                    if (Input::IsKeyDown(SDL_SCANCODE_D))
                        moveInput += 1.0f;

                    const bool jet =
                        Input::IsKeyDown(SDL_SCANCODE_W) ||
                        Input::IsKeyDown(SDL_SCANCODE_LSHIFT);

                    const bool crouch = Input::IsKeyDown(SDL_SCANCODE_S);

                    player.SetAim(mouseWorldX, mouseWorldY);

                    if (jumpQueued && player.IsGrounded())
                        Audio::Play(Sfx::Jump, 0.5f);

                    playerPrevFallSpeed = player.GetVelocityY();
                    playerWasGrounded = player.IsGrounded();

                    player.Update(
                        terrain,
                        &particles,
                        FixedTimeStep,
                        moveInput,
                        Input::IsKeyDown(SDL_SCANCODE_SPACE),
                        jet,
                        crouch);

                    if (!playerWasGrounded &&
                        player.IsGrounded() &&
                        playerPrevFallSpeed > 320.0f)
                    {
                        Audio::Play(Sfx::Land, 0.6f);

                        // Landing kicks up a dust ring and a camera thump
                        // scaled by how hard the fall was.
                        const float impact = std::min(
                            1.0f, playerPrevFallSpeed / 900.0f);

                        trauma = std::min(trauma + 0.12f + impact * 0.2f, 1.0f);

                        for (int i = 0; i < 7; i++)
                        {
                            particles.SpawnDust(
                                player.GetCenterX() + unit(rng) * 10.0f,
                                player.GetY() + player.GetHeight() + 20.0f,
                                unit(rng) * 60.0f,
                                -20.0f - (i % 3) * 12.0f);
                        }
                    }

                    // Footstep puffs on each fresh foot plant while
                    // actually walking.
                    for (int leg = 0; leg < 2; leg++)
                    {
                        const bool planted = player.GetLeg(leg).IsPlanted();

                        if (planted && !prevLegPlanted[leg] &&
                            std::fabs(player.GetVelocityX()) > 40.0f &&
                            player.IsGrounded() &&
                            !player.IsCrouching())
                        {
                            particles.SpawnDust(
                                player.GetLeg(leg).GetFootX(),
                                player.GetLeg(leg).GetFootY() - 1.0f,
                                -player.GetVelocityX() * 0.08f,
                                -16.0f);
                        }

                        prevLegPlanted[leg] = planted;
                    }

                    if (player.IsJetting() && jetSoundTimer <= 0.0f)
                    {
                        Audio::Play(Sfx::JetBurst, 0.55f);
                        jetSoundTimer = 0.15f;
                    }

                    player.GetWeapon().Update(FixedTimeStep);

                    if (Input::IsMouseButtonDown(SDL_BUTTON_LEFT) &&
                        !uiCapturingInput &&
                        !player.IsArmDestroyed())
                    {
                        if (player.GetWeapon().TryFire(
                            particles,
                            terrain,
                            player.GetMuzzleX(),
                            player.GetMuzzleY(),
                            player.GetAimDirX(),
                            player.GetAimDirY(),
                            mouseWorldX,
                            mouseWorldY,
                            &player))
                        {
                            player.NotifyFired();

                            const WeaponDef& def =
                                *player.GetWeapon().GetDef();

                            if (def.Kind == WeaponKind::Gun)
                                crosshairBloom = 1.0f;

                            // Dig tools tick fast; don't machine-gun the
                            // crumble sound.
                            if (def.Kind == WeaponKind::Gun)
                            {
                                Audio::Play(FireSfxFor(def), 0.8f);
                            }
                            else if (toolSfxTimer <= 0.0f)
                            {
                                Audio::Play(FireSfxFor(def), 0.7f);
                                toolSfxTimer = 0.1f;
                            }

                            // The digger can also maim: whatever limb its
                            // impact point lands on takes LimbDamage. The
                            // raycast here mirrors what TryFire just did
                            // internally (same swept direction, no time
                            // has passed), so the hit point matches.
                            if (def.Kind == WeaponKind::Digger &&
                                def.LimbDamage > 0)
                            {
                                float sweptDirX = 0.0f;
                                float sweptDirY = 0.0f;
                                player.GetWeapon().GetSweptDirection(
                                    player.GetAimDirX(),
                                    player.GetAimDirY(),
                                    sweptDirX,
                                    sweptDirY);

                                const float reachBehind =
                                    def.BarrelLength + 4.0f;

                                float digHitX = 0.0f;
                                float digHitY = 0.0f;

                                if (terrain.RaycastSolid(
                                    player.GetMuzzleX() -
                                        sweptDirX * reachBehind,
                                    player.GetMuzzleY() -
                                        sweptDirY * reachBehind,
                                    sweptDirX,
                                    sweptDirY,
                                    def.DigRange + reachBehind,
                                    digHitX,
                                    digHitY))
                                {
                                    for (Enemy& enemy : enemies)
                                    {
                                        if (!enemy.Body ||
                                            !enemy.Body->IsAlive())
                                            continue;

                                        BodyPartId hitPart =
                                            BodyPartId::None;
                                        float local01X = 0.0f;
                                        float local01Y = 0.0f;

                                        if (enemy.Body->TestLimbHit(
                                            digHitX, digHitY,
                                            hitPart, local01X, local01Y))
                                        {
                                            enemy.Body->TakeLimbDamage(
                                                hitPart,
                                                def.LimbDamage,
                                                local01X,
                                                local01Y,
                                                &particles,
                                                -sweptDirX * 40.0f,
                                                -sweptDirY * 40.0f);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (throwQueued && !uiCapturingInput &&
                        grenadeCooldown <= 0.0f)
                    {
                        grenades.Throw(
                            player.GetHandX(),
                            player.GetHandY(),
                            player.GetAimDirX() * 520.0f +
                                player.GetVelocityX() * 0.4f,
                            player.GetAimDirY() * 520.0f - 60.0f);

                        grenadeCooldown = GrenadeCooldownTime;

                        Audio::Play(Sfx::Throw, 0.7f);
                    }
                }
                else
                {
                    respawnTimer -= FixedTimeStep;

                    if (respawnTimer <= 0.0f)
                    {
                        player.ResetVitals();
                        player.Spawn(terrain, static_cast<float>(worldWidth) * 0.5f);
                        playerGibbed = false;
                    }
                }

                // --- Actor roster for collisions ---
                Actor* actorPtrs[MaxEnemies + MaxAllies + 1];
                int actorCount = 0;

                if (player.IsAlive())
                    actorPtrs[actorCount++] = &player;

                for (Enemy& enemy : enemies)
                    actorPtrs[actorCount++] = enemy.Body.get();

                for (Ally& ally : allies)
                    actorPtrs[actorCount++] = ally.Body.get();

                // --- Enemies ---
                for (Enemy& enemy : enemies)
                {
                    enemy.Brain.Update(
                        *enemy.Body,
                        player.IsAlive() ? &player : nullptr,
                        terrain,
                        particles,
                        terrain,
                        FixedTimeStep);
                }

                // --- Allies: each fights whichever enemy is nearest ---
                for (Ally& ally : allies)
                {
                    Actor* nearestEnemy = nullptr;
                    float nearestDistSquared = 0.0f;

                    for (Enemy& enemy : enemies)
                    {
                        if (!enemy.Body->IsAlive())
                            continue;

                        const float dx = enemy.Body->GetCenterX() -
                            ally.Body->GetCenterX();
                        const float dy = enemy.Body->GetCenterY() -
                            ally.Body->GetCenterY();
                        const float distSquared = dx * dx + dy * dy;

                        if (!nearestEnemy || distSquared < nearestDistSquared)
                        {
                            nearestEnemy = enemy.Body.get();
                            nearestDistSquared = distSquared;
                        }
                    }

                    ally.Brain.Update(
                        *ally.Body,
                        nearestEnemy,
                        terrain,
                        particles,
                        terrain,
                        FixedTimeStep);
                }

                // --- Grenades and particles ---
                grenades.Update(
                    terrain,
                    particles,
                    actorPtrs,
                    actorCount,
                    FixedTimeStep);

                if (grenades.ExplodedThisFrame())
                {
                    Audio::Play(Sfx::Explosion);
                    trauma = std::min(trauma + 0.5f, 1.0f);
                }

                trauma = std::max(0.0f, trauma - 1.6f * FixedTimeStep);

                particles.Update(
                    terrain,
                    FixedTimeStep,
                    actorPtrs,
                    actorCount);

                // Floating damage numbers: compare each enemy's health to
                // last tick's - catches every damage source (bullets,
                // explosions, digger limb damage) without any plumbing.
                for (Enemy& enemy : enemies)
                {
                    const int health = enemy.Body->GetHealth();

                    if (health < enemy.LastHealth)
                    {
                        // The killing blow counts too - clamp the shown
                        // damage to the health that actually remained
                        // (a 9999 both-legs-gone kill shouldn't print
                        // "9999").
                        const int delta =
                            enemy.LastHealth - std::max(0, health);

                        if (delta > 0)
                        {
                            floaters.push_back({
                                enemy.Body->GetCenterX() +
                                    unit(rng) * 5.0f,
                                enemy.Body->GetY() - 8.0f,
                                -34.0f,
                                0.75f, 0.75f,
                                2.0f,
                                std::to_string(delta),
                                255, 224, 130,
                                false });
                        }
                    }

                    enemy.LastHealth = health;
                }

                // Gold popups, batched: while gold keeps arriving the
                // window extends; once it goes quiet the total pops up.
                {
                    const int gold = player.GetGold();

                    if (gold > lastPlayerGold)
                    {
                        goldPopupAccum += gold - lastPlayerGold;
                        goldPopupTimer = 0.4f;
                    }

                    lastPlayerGold = gold;

                    if (goldPopupTimer > 0.0f)
                    {
                        goldPopupTimer -= FixedTimeStep;

                        if (goldPopupTimer <= 0.0f && goldPopupAccum > 0)
                        {
                            floaters.push_back({
                                player.GetCenterX(),
                                player.GetY() - 14.0f,
                                -30.0f,
                                1.0f, 1.0f,
                                2.0f,
                                "+" + std::to_string(goldPopupAccum) + "G",
                                235, 200, 90,
                                false });

                            goldPopupAccum = 0;
                        }
                    }
                }

                // --- Deaths ---
                if (!player.IsAlive() && !playerGibbed)
                {
                    player.Gib(particles);
                    Audio::Play(Sfx::Gib);
                    respawnTimer = RespawnTime;
                    playerGibbed = true;
                }

                for (std::size_t i = 0; i < enemies.size(); )
                {
                    if (!enemies[i].Body->IsAlive())
                    {
                        enemies[i].Body->Gib(particles);
                        Audio::Play(Sfx::Gib, 0.8f);

                        // Kill punch: brief hit-stop plus a camera
                        // thump - but only for kills the player can
                        // actually see. An ally finishing someone off
                        // across the map shouldn't freeze the world for
                        // no visible reason.
                        const float killDx =
                            enemies[i].Body->GetCenterX() -
                            player.GetCenterX();
                        const float killDy =
                            enemies[i].Body->GetCenterY() -
                            player.GetCenterY();

                        if (std::fabs(killDx) < 720.0f &&
                            std::fabs(killDy) < 440.0f)
                        {
                            hitStop = std::min(hitStop + 0.05f, 0.09f);
                            trauma = std::min(trauma + 0.18f, 1.0f);
                        }

                        // Sometimes drop a medkit.
                        if (unit(rng) > 0.2f)
                        {
                            pickups.push_back({
                                enemies[i].Body->GetCenterX(),
                                enemies[i].Body->GetCenterY(),
                                -120.0f });
                        }

                        enemies.erase(enemies.begin() +
                            static_cast<std::ptrdiff_t>(i));
                    }
                    else
                    {
                        i++;
                    }
                }

                for (std::size_t i = 0; i < allies.size(); )
                {
                    if (!allies[i].Body->IsAlive())
                    {
                        allies[i].Body->Gib(particles);
                        Audio::Play(Sfx::Gib, 0.8f);

                        allies.erase(allies.begin() +
                            static_cast<std::ptrdiff_t>(i));
                    }
                    else
                    {
                        i++;
                    }
                }

                // --- Medkit pickups ---
                for (std::size_t i = 0; i < pickups.size(); )
                {
                    HealthPickup& pickup = pickups[i];

                    pickup.VelY += 900.0f * FixedTimeStep;

                    const float fall = pickup.VelY * FixedTimeStep;

                    if (!terrain.IsSolid(pickup.X, pickup.Y + fall + 4.0f))
                        pickup.Y += fall;
                    else
                        pickup.VelY = 0.0f;

                    const float dx = pickup.X - player.GetCenterX();
                    const float dy = pickup.Y - player.GetCenterY();

                    if (player.IsAlive() &&
                        dx * dx + dy * dy < 34.0f * 34.0f)
                    {
                        player.Heal(30);
                        Audio::Play(Sfx::Reload, 0.9f);

                        pickups[i] = pickups.back();
                        pickups.pop_back();
                    }
                    else
                    {
                        i++;
                    }
                }

                // --- Waves ---
                if (botsEnabled && enemies.empty())
                {
                    waveTimer -= FixedTimeStep;

                    if (waveTimer <= 0.0f)
                    {
                        wave++;
                        spawnWave(std::min(1 + wave, MaxEnemies));
                        waveTimer = 4.0f;

                        // Center-screen wave banner with a sub-line.
                        const std::string banner =
                            "WAVE " + std::to_string(wave);

                        floaters.push_back({
                            (static_cast<float>(ViewWidth) -
                                PixelFont::Measure(banner, 6.0f)) * 0.5f,
                            170.0f,
                            -9.0f,
                            2.2f, 2.2f,
                            6.0f,
                            banner,
                            240, 235, 220,
                            true });

                        floaters.push_back({
                            (static_cast<float>(ViewWidth) -
                                PixelFont::Measure("HOSTILES INBOUND", 2.0f)) *
                                0.5f,
                            212.0f,
                            -9.0f,
                            2.2f, 2.2f,
                            2.0f,
                            "HOSTILES INBOUND",
                            255, 130, 110,
                            true });
                    }
                }

                // --- Deliveries: drop ship flies in, payload parachutes
                // down, then delivers its contents on landing. The
                // system only owns the flight/fall physics; the actual
                // gameplay effect happens here, where Actor/player
                // context is available.
                {
                    std::vector<const Delivery*> landed;
                    std::vector<const Delivery*> delivered;

                    deliveries.Update(terrain, FixedTimeStep, landed, delivered);

                    if (!landed.empty())
                        Audio::Play(Sfx::Land, 0.7f);

                    for (const Delivery* delivery : delivered)
                    {
                        switch (delivery->Kind)
                        {
                        case DeliveryKind::SupplyCrate:
                            if (player.IsAlive())
                            {
                                player.Heal(50);
                                player.GetWeapon().Refill();
                            }
                            break;

                        case DeliveryKind::Reinforcement:
                            spawnAlly(delivery->PayloadX, delivery->Weapon);
                            break;

                        case DeliveryKind::WeaponPickup:
                            weaponPickups.push_back({
                                delivery->PayloadX,
                                delivery->PayloadY,
                                0.0f,
                                delivery->Weapon });
                            break;
                        }
                    }
                }

                // --- Weapon field pickups: settle to the ground, then
                // wait for the player to walk over them ---
                for (std::size_t i = 0; i < weaponPickups.size(); )
                {
                    WeaponPickup& pickup = weaponPickups[i];

                    pickup.VelY += 900.0f * FixedTimeStep;

                    const float fall = pickup.VelY * FixedTimeStep;

                    if (!terrain.IsSolid(pickup.X, pickup.Y + fall + 4.0f))
                        pickup.Y += fall;
                    else
                        pickup.VelY = 0.0f;

                    const float dx = pickup.X - player.GetCenterX();
                    const float dy = pickup.Y - player.GetCenterY();

                    if (player.IsAlive() && pickup.Weapon &&
                        dx * dx + dy * dy < 30.0f * 30.0f)
                    {
                        player.SetWeaponDef(pickup.Weapon);

                        // Keep the number-key loadout and HUD slot
                        // highlight in sync with whatever got equipped.
                        for (int slot = 0; slot < LoadoutSize; slot++)
                        {
                            if (loadout[slot] == pickup.Weapon)
                            {
                                currentWeapon = slot;
                                break;
                            }
                        }

                        Audio::Play(Sfx::Reload, 0.8f);

                        weaponPickups[i] = weaponPickups.back();
                        weaponPickups.pop_back();
                    }
                    else
                    {
                        i++;
                    }
                }

                accumulator -= FixedTimeStep;
                simTicked = true;
            }

            // Latched input edges are consumed once the sim has actually
            // seen them; a frame with zero ticks (hit-stop) keeps them
            // queued for the next real tick.
            if (simTicked)
            {
                throwQueued = false;
                jumpQueued = false;
            }

            terrain.Update();

            // --- Camera: smoothed follow with aim lookahead and
            // trauma shake ---
            //
            // The target leads toward where the player is aiming so the
            // view shows more of what matters; exponential smoothing
            // makes every movement ease instead of hard-locking; shake
            // is smooth multi-frequency noise scaled by trauma^2 rather
            // than raw per-frame jitter.
            {
                const float lookX =
                    player.IsAlive() ? player.GetAimDirX() * 105.0f : 0.0f;
                const float lookY =
                    player.IsAlive() ? player.GetAimDirY() * 55.0f : 0.0f;

                const float targetX = std::clamp(
                    player.GetCenterX() + lookX - ViewWidth * 0.5f,
                    0.0f,
                    static_cast<float>(worldWidth - ViewWidth));
                const float targetY = std::clamp(
                    player.GetCenterY() + lookY - ViewHeight * 0.5f,
                    0.0f,
                    static_cast<float>(worldHeight - ViewHeight));

                // A respawn far away snaps instead of panning across the
                // whole map.
                if (std::fabs(targetX - camSmoothX) > 620.0f ||
                    std::fabs(targetY - camSmoothY) > 480.0f)
                {
                    camSmoothX = targetX;
                    camSmoothY = targetY;
                }

                const float blend =
                    1.0f - std::exp(-frameTime * 7.0f);

                camSmoothX += (targetX - camSmoothX) * blend;
                camSmoothY += (targetY - camSmoothY) * blend;

                const float amp = trauma * trauma * 26.0f;

                const float shakeX = amp *
                    (0.6f * std::sin(timeSeconds * 41.0f) +
                     0.4f * std::sin(timeSeconds * 23.7f + 1.3f));
                const float shakeY = amp *
                    (0.6f * std::sin(timeSeconds * 37.3f + 2.1f) +
                     0.4f * std::sin(timeSeconds * 19.1f));

                camera.SetPosition(
                    std::clamp(
                        camSmoothX + shakeX,
                        0.0f,
                        static_cast<float>(worldWidth - ViewWidth)),
                    std::clamp(
                        camSmoothY + shakeY,
                        0.0f,
                        static_cast<float>(worldHeight - ViewHeight)));

                // UI anchoring tracks the smoothed position only - no
                // shake - so menus hold still while the world rocks.
                uiCamX = std::clamp(
                    camSmoothX, 0.0f,
                    static_cast<float>(worldWidth - ViewWidth));
                uiCamY = std::clamp(
                    camSmoothY, 0.0f,
                    static_cast<float>(worldHeight - ViewHeight));
            }

            // Damage feedback: vignette pulse plus a camera thump.
            if (player.GetHealth() < lastPlayerHealth)
            {
                hurtVignette = 0.6f;
                trauma = std::min(trauma + 0.3f, 1.0f);
            }

            lastPlayerHealth = player.GetHealth();
            hurtVignette = std::max(0.0f, hurtVignette - frameTime);

            // Visual-decay state for the HUD and crosshair.
            crosshairBloom = std::max(0.0f, crosshairBloom - frameTime * 6.0f);

            {
                const float healthFill =
                    std::max(0, player.GetHealth()) / 100.0f;

                if (healthFill >= healthGhost)
                    healthGhost = healthFill;
                else
                    healthGhost = std::max(
                        healthFill, healthGhost - frameTime * 0.4f);
            }

            // Floating text drifts and fades in render time.
            for (std::size_t i = 0; i < floaters.size(); )
            {
                Floater& floater = floaters[i];

                floater.Y += floater.VelY * frameTime;
                floater.Life -= frameTime;

                if (floater.Life <= 0.0f)
                {
                    floaters[i] = floaters.back();
                    floaters.pop_back();
                }
                else
                {
                    i++;
                }
            }

            m_Window.BeginFrame();

            background.Draw(
                m_Window,
                camera.GetX(),
                camera.GetY(),
                ViewWidth,
                ViewHeight,
                timeSeconds);

            terrain.Draw(m_Window);

            // Medkits.
            for (const HealthPickup& pickup : pickups)
            {
                m_Window.DrawFilledRect(
                    pickup.X - 5.0f, pickup.Y - 4.0f, 10.0f, 8.0f,
                    235, 235, 235, 255);
                m_Window.DrawFilledRect(
                    pickup.X - 1.0f, pickup.Y - 3.0f, 2.0f, 6.0f,
                    205, 60, 50, 255);
                m_Window.DrawFilledRect(
                    pickup.X - 3.0f, pickup.Y - 1.0f, 6.0f, 2.0f,
                    205, 60, 50, 255);
            }

            // Weapon field pickups: a small olive case with a glow so it
            // reads as grabbable, plus its weapon name floating above.
            for (const WeaponPickup& pickup : weaponPickups)
            {
                m_Window.DrawGlow(
                    pickup.X, pickup.Y - 4.0f, 14.0f, 200, 220, 160, 70);

                m_Window.DrawFilledRect(
                    pickup.X - 6.0f, pickup.Y - 6.0f, 12.0f, 8.0f,
                    24, 22, 27, 255);
                m_Window.DrawFilledRect(
                    pickup.X - 5.0f, pickup.Y - 5.0f, 10.0f, 6.0f,
                    72, 82, 56, 255);
                m_Window.DrawFilledRect(
                    pickup.X - 5.0f, pickup.Y - 5.0f, 10.0f, 1.5f,
                    92, 104, 70, 255);

                if (pickup.Weapon)
                {
                    std::string name = pickup.Weapon->Name;

                    for (char& c : name)
                        c = static_cast<char>(std::toupper(
                            static_cast<unsigned char>(c)));

                    PixelFont::Draw(
                        m_Window,
                        pickup.X - PixelFont::Measure(name, 1.5f) * 0.5f,
                        pickup.Y - 18.0f,
                        1.5f, name, 210, 225, 235);
                }
            }

            grenades.Draw(m_Window);

            deliveries.Draw(m_Window);

            for (Enemy& enemy : enemies)
                enemy.Body->Draw(m_Window);

            for (Ally& ally : allies)
                ally.Body->Draw(m_Window);

            if (player.IsAlive())
                player.Draw(m_Window);

            particles.Draw(m_Window);

            // --- Floating combat text (damage numbers, gold, banners) ---
            for (const Floater& floater : floaters)
            {
                // Hold full alpha, then fade over the last 40%.
                const float lifeFrac = floater.Life / floater.MaxLife;
                const Uint8 alpha = static_cast<Uint8>(
                    255.0f * std::min(1.0f, lifeFrac / 0.4f));

                const float x = floater.ScreenSpace
                    ? floater.X
                    : floater.X - camera.GetX() -
                        PixelFont::Measure(floater.Text, floater.Scale) * 0.5f;
                const float y = floater.ScreenSpace
                    ? floater.Y
                    : floater.Y - camera.GetY();

                PixelFont::Draw(
                    m_Window,
                    x + floater.Scale * 0.5f, y + floater.Scale * 0.5f,
                    floater.Scale, floater.Text,
                    10, 10, 14,
                    static_cast<Uint8>(alpha * 3 / 5));
                PixelFont::Draw(
                    m_Window, x, y, floater.Scale, floater.Text,
                    floater.R, floater.G, floater.B, alpha);
            }

            // --- Dynamic crosshair: gap opens with weapon spread and
            // blooms on each shot; a reload bar replaces confusion about
            // why firing stopped. Drawn in SCREEN space at the raw mouse
            // position so it can never detach from the cursor, no matter
            // what the camera (smoothing, shake) is doing this frame ---
            {
                const float cursorX = Input::GetMouseX();
                const float cursorY = Input::GetMouseY();

                const WeaponDef* def = player.GetWeapon().GetDef();

                const float spread =
                    def ? def->SpreadDegrees : 3.0f;

                const float gap =
                    5.0f + spread * 1.1f + crosshairBloom * 7.0f;

                const Uint8 alpha = 205;

                // Four ticks around an open center.
                m_Window.DrawScreenRect(
                    cursorX - gap - 5.0f, cursorY - 1.0f,
                    5.0f, 2.0f, 255, 255, 255, alpha);
                m_Window.DrawScreenRect(
                    cursorX + gap, cursorY - 1.0f,
                    5.0f, 2.0f, 255, 255, 255, alpha);
                m_Window.DrawScreenRect(
                    cursorX - 1.0f, cursorY - gap - 5.0f,
                    2.0f, 5.0f, 255, 255, 255, alpha);
                m_Window.DrawScreenRect(
                    cursorX - 1.0f, cursorY + gap,
                    2.0f, 5.0f, 255, 255, 255, alpha);

                m_Window.DrawScreenRect(
                    cursorX - 1.0f, cursorY - 1.0f,
                    2.0f, 2.0f, 255, 255, 255, alpha);

                // Reload progress right under the cursor.
                if (player.GetWeapon().IsReloading())
                {
                    const float progress =
                        player.GetWeapon().GetReloadProgress();

                    m_Window.DrawScreenRect(
                        cursorX - 11.0f, cursorY + gap + 8.0f,
                        22.0f, 3.0f, 20, 20, 26, 200);
                    m_Window.DrawScreenRect(
                        cursorX - 11.0f, cursorY + gap + 8.0f,
                        22.0f * progress, 3.0f, 230, 150, 60, 235);
                }
            }

            // --- Floating buy menu (Tab held), anchored above the player ---
            //
            // Drawn with the exact screen coordinates computed earlier
            // this frame for click detection, via DrawScreenRect, so the
            // clickable area and the visible menu are always identical.
            if (menuHeld)
            {
                // A thin connector from the menu down to the player's
                // head, so it reads as anchored rather than floating
                // free.
                m_Window.DrawScreenRect(
                    menuAnchorX - uiCameraX - 1.0f,
                    menuAnchorY - uiCameraY + WheelButtonH * 0.5f,
                    2.0f,
                    (player.GetY() - uiCameraY) -
                        (menuAnchorY - uiCameraY + WheelButtonH * 0.5f),
                    150, 190, 230, 90);

                if (!buyPanelOpen)
                {
                    // Three-slot wheel: BUY is live, the rest are
                    // placeholders for future menu options.
                    const char* labels[3] = { "BUY", "GEAR", "CALL" };
                    const bool enabled[3] = { true, false, false };

                    for (int i = 0; i < 3; i++)
                    {
                        const float slotX = wheelScreenX +
                            static_cast<float>(i) *
                                (WheelButtonW + WheelGap);

                        m_Window.DrawScreenRect(
                            slotX - 1.0f, wheelScreenY - 1.0f,
                            WheelButtonW + 2.0f, WheelButtonH + 2.0f,
                            120, 126, 148, 90);

                        m_Window.DrawScreenRect(
                            slotX, wheelScreenY,
                            WheelButtonW, WheelButtonH,
                            enabled[i] ? 30 : 16,
                            enabled[i] ? 34 : 16,
                            enabled[i] ? 30 : 20,
                            230);

                        if (enabled[i])
                        {
                            m_Window.DrawScreenRect(
                                slotX, wheelScreenY,
                                WheelButtonW, 3.0f,
                                230, 205, 110, 220);
                        }

                        const std::string label = labels[i];
                        const Uint8 textShade =
                            enabled[i] ? 235 : 110;

                        PixelFont::Draw(
                            m_Window,
                            slotX + (WheelButtonW -
                                PixelFont::Measure(label, 2.0f)) * 0.5f,
                            wheelScreenY + WheelButtonH * 0.5f - 5.0f,
                            2.0f,
                            label,
                            textShade, textShade,
                            enabled[i] ? 200 : textShade);
                    }
                }
                else
                {
                    // Order panel: a real catalog + cart, Cortex Command
                    // style. Buying a weapon equips whichever body is
                    // "active" (highlighted in the cart below); with no
                    // active body it queues as a loose field pickup.
                    m_Window.DrawScreenRect(
                        panelScreenX - 1.0f, panelScreenY - 1.0f,
                        BuyPanelW + 2.0f, BuyPanelH + 2.0f,
                        120, 126, 148, 100);
                    m_Window.DrawScreenRect(
                        panelScreenX, panelScreenY,
                        BuyPanelW, BuyPanelH,
                        14, 14, 20, 235);

                    PixelFont::Draw(
                        m_Window,
                        panelScreenX + 10.0f, panelScreenY + 10.0f,
                        2.0f, "ORDER SUPPLIES", 235, 235, 240);

                    const std::string goldLine =
                        "GOLD: " + std::to_string(player.GetGold());

                    PixelFont::Draw(
                        m_Window,
                        panelScreenX + 10.0f, panelScreenY + 26.0f,
                        2.0f, goldLine, 230, 200, 110);

                    m_Window.DrawScreenRect(
                        panelScreenX + 8.0f, panelScreenY + 40.0f,
                        BuyPanelW - 16.0f, 1.0f,
                        120, 126, 148, 90);

                    // --- Catalog rows ---
                    for (std::size_t i = 0; i < catalog.size(); i++)
                    {
                        const CatalogEntry& entry = catalog[i];
                        const float rowY = panelScreenY + catalogRowOffset[i];

                        const bool roomForBody =
                            entry.Kind != CatalogKind::Body ||
                            static_cast<int>(cart.GetBodies().size()) +
                                    static_cast<int>(allies.size()) <
                                MaxAllies;

                        const bool affordable = roomForBody &&
                            player.GetGold() >= cart.GetTotalCost() + entry.Cost;

                        Uint8 baseR = 26, baseG = 26, baseB = 34;

                        switch (entry.Kind)
                        {
                        case CatalogKind::Body:
                            baseR = 24; baseG = 30; baseB = 42; break;
                        case CatalogKind::Weapon:
                            baseR = 28; baseG = 28; baseB = 32; break;
                        case CatalogKind::Item:
                            baseR = 34; baseG = 30; baseB = 20; break;
                        }

                        m_Window.DrawScreenRect(
                            panelScreenX + 8.0f, rowY, BuyPanelW - 16.0f,
                            CatalogRowH,
                            affordable ? baseR : static_cast<Uint8>(baseR * 0.6f),
                            affordable ? baseG : static_cast<Uint8>(baseG * 0.6f),
                            affordable ? baseB : static_cast<Uint8>(baseB * 0.6f),
                            210);

                        const std::string label =
                            entry.Name + " " + std::to_string(entry.Cost) + "G";

                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + 12.0f, rowY + 4.0f,
                            1.5f, label,
                            affordable ? 225 : 110,
                            affordable ? 228 : 110,
                            affordable ? 235 : 114);
                    }

                    // --- Assignment toggle ---
                    {
                        const float assignY = panelScreenY + assignRowOffset;
                        const int activeBody = cart.GetActiveBodyIndex();

                        std::string assignLabel = "EQUIP: LOOSE ITEM";

                        if (activeBody >= 0 &&
                            activeBody < static_cast<int>(cart.GetBodies().size()))
                        {
                            assignLabel = "EQUIP: RIFLEMAN #" +
                                std::to_string(activeBody + 1);
                        }

                        m_Window.DrawScreenRect(
                            panelScreenX + 8.0f, assignY, BuyPanelW - 16.0f,
                            CatalogRowH, 46, 50, 60, 220);

                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + 12.0f, assignY + 4.0f,
                            1.5f, assignLabel, 200, 210, 225);
                    }

                    // --- Cart summary ---
                    const std::vector<CartLine> cartLines = buildCartLines();

                    for (std::size_t i = 0;
                        i < cartLines.size() &&
                            i < static_cast<std::size_t>(MaxCartLinesShown);
                        i++)
                    {
                        const float lineY = panelScreenY +
                            cartLinesStartOffset +
                            static_cast<float>(i) * CartLineH;

                        const bool isActiveBody =
                            cartLines[i].Kind == 0 &&
                            cartLines[i].RefIndex == cart.GetActiveBodyIndex();

                        if (isActiveBody)
                        {
                            m_Window.DrawScreenRect(
                                panelScreenX + 8.0f, lineY,
                                BuyPanelW - 16.0f, CartLineH,
                                70, 84, 60, 160);
                        }

                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + 12.0f, lineY + 2.0f,
                            1.5f, cartLines[i].Text,
                            isActiveBody ? 220 : 175,
                            isActiveBody ? 235 : 177,
                            isActiveBody ? 200 : 180);
                    }

                    if (cartLines.empty())
                    {
                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + 12.0f,
                            panelScreenY + cartLinesStartOffset + 2.0f,
                            1.5f, "CART IS EMPTY", 120, 122, 130);
                    }
                    else if (cartLines.size() >
                        static_cast<std::size_t>(MaxCartLinesShown))
                    {
                        const std::string more = "+" + std::to_string(
                            cartLines.size() - MaxCartLinesShown) + " MORE";

                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + 12.0f,
                            panelScreenY + cartLinesStartOffset +
                                static_cast<float>(MaxCartLinesShown) *
                                    CartLineH,
                            1.3f, more, 140, 142, 150);
                    }

                    // --- Total and action buttons ---
                    {
                        const bool canAfford =
                            player.GetGold() >= cart.GetTotalCost();

                        const std::string totalLine =
                            "TOTAL: " + std::to_string(cart.GetTotalCost()) + "G";

                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + 12.0f,
                            panelScreenY + totalLineOffset,
                            1.5f, totalLine,
                            canAfford ? 230 : 220,
                            canAfford ? 200 : 90,
                            canAfford ? 110 : 90);

                        const bool canSend = !cart.IsEmpty() && canAfford;
                        const float sendY = panelScreenY + sendButtonOffset;

                        m_Window.DrawScreenRect(
                            panelScreenX + 8.0f, sendY, BuyPanelW - 16.0f,
                            SendButtonH,
                            canSend ? 40 : 26, canSend ? 110 : 26,
                            canSend ? 56 : 32, 235);

                        const std::string sendLabel = "SEND ORDER";

                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + BuyPanelW * 0.5f -
                                PixelFont::Measure(sendLabel, 2.0f) * 0.5f,
                            sendY + 7.0f,
                            2.0f, sendLabel,
                            canSend ? 230 : 130, canSend ? 235 : 132,
                            canSend ? 230 : 132);

                        const bool canClear = !cart.IsEmpty();
                        const float clearY = panelScreenY + clearButtonOffset;

                        m_Window.DrawScreenRect(
                            panelScreenX + 8.0f, clearY, BuyPanelW - 16.0f,
                            ClearButtonH,
                            canClear ? 90 : 40, canClear ? 46 : 30,
                            canClear ? 46 : 30, 220);

                        const std::string clearLabel = "CLEAR";

                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + BuyPanelW * 0.5f -
                                PixelFont::Measure(clearLabel, 1.5f) * 0.5f,
                            clearY + 4.0f,
                            1.5f, clearLabel,
                            canClear ? 230 : 130,
                            canClear ? 190 : 110,
                            canClear ? 190 : 110);
                    }

                    // Close button.
                    m_Window.DrawScreenRect(
                        closeScreenX, closeScreenY,
                        CloseButtonSize, CloseButtonSize,
                        70, 40, 40, 230);

                    PixelFont::Draw(
                        m_Window,
                        closeScreenX + 5.0f, closeScreenY + 5.0f,
                        2.0f, "X", 235, 200, 200);
                }
            }

            // --- Screen-edge vignette (always) + damage feedback ---
            {
                const float edge = 90.0f;

                m_Window.DrawScreenRect(0.0f, 0.0f,
                    static_cast<float>(ViewWidth), edge * 0.5f, 0, 0, 8, 46);
                m_Window.DrawScreenRect(0.0f,
                    static_cast<float>(ViewHeight) - edge * 0.5f,
                    static_cast<float>(ViewWidth), edge * 0.5f, 0, 0, 8, 46);
                m_Window.DrawScreenRect(0.0f, 0.0f,
                    edge * 0.5f, static_cast<float>(ViewHeight), 0, 0, 8, 46);
                m_Window.DrawScreenRect(
                    static_cast<float>(ViewWidth) - edge * 0.5f, 0.0f,
                    edge * 0.5f, static_cast<float>(ViewHeight), 0, 0, 8, 46);

                float redPulse = hurtVignette / 0.6f;

                // Low health: constant heartbeat pulse.
                if (player.IsAlive() && player.GetHealth() < 30)
                {
                    redPulse = std::max(
                        redPulse,
                        0.25f + 0.2f * std::sin(timeSeconds * 6.0f));
                }

                if (redPulse > 0.0f)
                {
                    const Uint8 alpha = static_cast<Uint8>(90.0f * redPulse);
                    const float band = 70.0f;

                    m_Window.DrawScreenRect(0.0f, 0.0f,
                        static_cast<float>(ViewWidth), band, 160, 20, 20, alpha);
                    m_Window.DrawScreenRect(0.0f,
                        static_cast<float>(ViewHeight) - band,
                        static_cast<float>(ViewWidth), band, 160, 20, 20, alpha);
                    m_Window.DrawScreenRect(0.0f, 0.0f,
                        band, static_cast<float>(ViewHeight), 160, 20, 20, alpha);
                    m_Window.DrawScreenRect(
                        static_cast<float>(ViewWidth) - band, 0.0f,
                        band, static_cast<float>(ViewHeight), 160, 20, 20, alpha);
                }
            }

            // --- HUD ---
            const float barWidth = 190.0f;
            const float barX = 32.0f;

            // Screen-space text with a soft drop shadow so it stays
            // readable over any scene.
            auto drawText = [&](
                float x, float y, float scale,
                const std::string& text,
                Uint8 r, Uint8 g, Uint8 b)
            {
                PixelFont::Draw(m_Window,
                    x + scale * 0.5f, y + scale * 0.5f,
                    scale, text, 10, 10, 14, 150);
                PixelFont::Draw(m_Window, x, y, scale, text, r, g, b);
            };

            // Backing panel with a subtle border and a highlight line.
            m_Window.DrawScreenRect(8.0f, 8.0f, barWidth + 34.0f, 92.0f,
                120, 126, 148, 60);
            m_Window.DrawScreenRect(9.0f, 9.0f, barWidth + 32.0f, 90.0f,
                12, 12, 18, 195);
            m_Window.DrawScreenRect(9.0f, 9.0f, barWidth + 32.0f, 1.0f,
                170, 176, 198, 70);

            auto drawBar = [&](
                float y,
                float fill,
                Uint8 r, Uint8 g, Uint8 b,
                float ghost = -1.0f)
            {
                // Border and inset trough.
                m_Window.DrawScreenRect(barX - 1.0f, y - 1.0f,
                    barWidth + 2.0f, 11.0f, 52, 54, 66, 255);
                m_Window.DrawScreenRect(barX, y, barWidth, 9.0f,
                    24, 24, 32, 255);

                // Recent damage lingers as a dimmer ghost strip that the
                // real value visibly ate into.
                if (ghost > 0.0f)
                {
                    m_Window.DrawScreenRect(
                        barX, y, barWidth * ghost, 9.0f,
                        static_cast<Uint8>(r / 2 + 40),
                        static_cast<Uint8>(g / 3),
                        static_cast<Uint8>(b / 3),
                        220);
                }

                if (fill > 0.0f)
                {
                    // Fill with a light sheen on the top half and a
                    // darker base line.
                    m_Window.DrawScreenRect(barX, y, barWidth * fill, 9.0f,
                        r, g, b, 255);
                    m_Window.DrawScreenRect(barX, y, barWidth * fill, 4.0f,
                        255, 255, 255, 46);
                    m_Window.DrawScreenRect(barX, y + 8.0f, barWidth * fill,
                        1.0f, 0, 0, 0, 60);
                }
            };

            // Tiny icons in the gutter left of each bar.
            {
                // Health: red cross.
                m_Window.DrawScreenRect(18.0f, 15.0f, 8.0f, 2.0f,
                    225, 90, 80, 255);
                m_Window.DrawScreenRect(21.0f, 12.0f, 2.0f, 8.0f,
                    225, 90, 80, 255);

                // Fuel: small flame.
                m_Window.DrawScreenRect(20.0f, 26.0f, 4.0f, 6.0f,
                    240, 160, 70, 255);
                m_Window.DrawScreenRect(21.0f, 24.0f, 2.0f, 3.0f,
                    255, 210, 120, 255);

                // Ammo: bullet on its side.
                m_Window.DrawScreenRect(18.0f, 41.0f, 6.0f, 4.0f,
                    214, 178, 86, 255);
                m_Window.DrawScreenRect(24.0f, 42.0f, 2.0f, 2.0f,
                    150, 150, 160, 255);
            }

            drawBar(14.0f,
                std::max(0, player.GetHealth()) / 100.0f,
                205, 60, 50,
                healthGhost);

            drawBar(27.0f, player.GetFuel(), 90, 150, 230);

            const Weapon& weapon = player.GetWeapon();

            if (weapon.IsReloading())
                drawBar(40.0f, weapon.GetReloadProgress(), 230, 150, 60);
            else if (weapon.GetClipSize() > 0)
                drawBar(40.0f,
                    weapon.GetAmmo() /
                        static_cast<float>(weapon.GetClipSize()),
                    230, 210, 90);
            else
                drawBar(40.0f, 1.0f, 140, 145, 155);

            // Weapon/tool readout: name and ammo state.
            if (weapon.GetDef())
            {
                std::string info = weapon.GetDef()->Name;

                for (char& c : info)
                    c = static_cast<char>(std::toupper(
                        static_cast<unsigned char>(c)));

                drawText(barX, 55.0f, 2.0f, info, 235, 235, 240);

                std::string ammoText;

                if (weapon.IsReloading())
                    ammoText = "RELOADING";
                else if (weapon.GetClipSize() > 0)
                    ammoText = std::to_string(weapon.GetAmmo()) + "/" +
                        std::to_string(weapon.GetClipSize());
                else if (weapon.GetDef()->Kind == WeaponKind::Gun)
                    ammoText = "INF";
                else
                    ammoText = "TOOL";

                drawText(
                    barX + barWidth - PixelFont::Measure(ammoText, 2.0f),
                    55.0f,
                    2.0f,
                    ammoText,
                    weapon.IsReloading() ? 230 : 200,
                    weapon.IsReloading() ? 150 : 205,
                    weapon.IsReloading() ? 60 : 215);
            }

            // Weapon slots with numbers.
            for (int i = 0; i < LoadoutSize; i++)
            {
                const bool selected = i == currentWeapon;
                const float slotX = barX + static_cast<float>(i) * 22.0f;

                m_Window.DrawScreenRect(slotX, 70.0f, 18.0f, 16.0f,
                    28, 28, 36, 255);

                if (selected)
                {
                    m_Window.DrawScreenRect(slotX, 70.0f, 18.0f, 16.0f,
                        235, 225, 170, 255);
                    m_Window.DrawScreenRect(slotX + 1.0f, 71.0f,
                        16.0f, 14.0f, 62, 58, 40, 255);
                }

                PixelFont::Draw(
                    m_Window,
                    slotX + 6.0f,
                    73.0f,
                    2.0f,
                    std::to_string(i + 1),
                    selected ? 235 : 130,
                    selected ? 225 : 132,
                    selected ? 170 : 140);
            }

            // Gold readout: a small nugget icon plus the tally, sitting
            // to the right of the weapon slots in the same row.
            {
                const float goldX =
                    barX + static_cast<float>(LoadoutSize) * 22.0f + 6.0f;

                m_Window.DrawScreenRect(goldX, 73.0f, 8.0f, 8.0f,
                    24, 22, 27, 255);
                m_Window.DrawScreenRect(goldX + 1.0f, 74.0f, 6.0f, 6.0f,
                    218, 176, 56, 255);
                m_Window.DrawScreenRect(goldX + 1.0f, 74.0f, 6.0f, 2.0f,
                    240, 205, 110, 255);

                drawText(
                    goldX + 13.0f,
                    75.0f,
                    2.0f,
                    std::to_string(player.GetGold()),
                    230, 200, 110);
            }

            // Wave status (top right): wave number and enemies left.
            {
                const std::string waveText = "WAVE " + std::to_string(wave);

                drawText(
                    static_cast<float>(ViewWidth) - 20.0f -
                        PixelFont::Measure(waveText, 3.0f),
                    16.0f,
                    3.0f,
                    waveText,
                    235, 235, 240);

                for (std::size_t i = 0; i < enemies.size(); i++)
                {
                    m_Window.DrawScreenRect(
                        static_cast<float>(ViewWidth) - 22.0f -
                            static_cast<float>(i) * 14.0f,
                        36.0f,
                        10.0f,
                        10.0f,
                        255, 118, 106, 255);
                }
            }

            // Bots-spawn toggle button (brightens under the cursor so it
            // reads as clickable).
            {
                const Uint8 borderShade = botsButtonHovered ? 150 : 28;

                m_Window.DrawScreenRect(
                    BotsButtonX, BotsButtonY, BotsButtonW, BotsButtonH,
                    borderShade,
                    static_cast<Uint8>(borderShade + 4),
                    static_cast<Uint8>(borderShade + 14),
                    255);

                const int hoverBoost = botsButtonHovered ? 22 : 0;

                m_Window.DrawScreenRect(
                    BotsButtonX + 1.0f, BotsButtonY + 1.0f,
                    BotsButtonW - 2.0f, BotsButtonH - 2.0f,
                    static_cast<Uint8>((botsEnabled ? 42 : 92) + hoverBoost),
                    static_cast<Uint8>((botsEnabled ? 120 : 46) + hoverBoost),
                    static_cast<Uint8>((botsEnabled ? 58 : 46) + hoverBoost),
                    220);

                const std::string botsLabel =
                    botsEnabled ? "BOTS: ON" : "BOTS: OFF";

                drawText(
                    BotsButtonX +
                        (BotsButtonW - PixelFont::Measure(botsLabel, 2.0f)) *
                            0.5f,
                    BotsButtonY + 7.0f,
                    2.0f,
                    botsLabel,
                    235, 235, 240);
            }

            // Death overlay.
            if (!player.IsAlive())
            {
                m_Window.DrawScreenRect(
                    0.0f, 0.0f,
                    static_cast<float>(ViewWidth),
                    static_cast<float>(ViewHeight),
                    140, 20, 20, 60);

                m_Window.DrawScreenRect(
                    ViewWidth * 0.5f - 100.0f,
                    ViewHeight * 0.5f - 6.0f,
                    200.0f * std::clamp(
                        1.0f - respawnTimer / RespawnTime, 0.0f, 1.0f),
                    12.0f,
                    240, 240, 240, 220);
            }

            m_Window.EndFrame();
        }

        Audio::Shutdown();
    }
}
