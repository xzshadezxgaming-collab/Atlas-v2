#include "Application.h"

#include "../Actors/Actor.h"
#include "../Actors/AIController.h"
#include "../Audio/Audio.h"
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

        constexpr float BuyPanelW = 210.0f;
        constexpr float BuyPanelH = 132.0f;
        constexpr float CloseButtonSize = 18.0f;

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

        auto spawnAlly = [&](float x)
        {
            if (static_cast<int>(allies.size()) >= MaxAllies)
                return;

            Ally ally;
            ally.Body = std::make_unique<Actor>();
            ally.Body->LoadBodySprite(m_Window.GetRenderer(), playerSprite);
            ally.Body->SetTint(120, 170, 255);
            ally.Body->SetTeam(0);
            ally.Body->SetWeaponDef(allyWeapon);
            ally.Body->Spawn(terrain, x);
            allies.push_back(std::move(ally));
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

        float cameraShake = 0.0f;
        std::vector<HealthPickup> pickups;
        std::uniform_real_distribution<float> unit(-1.0f, 1.0f);

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

            const bool throwPressed =
                Input::WasMouseButtonPressed(SDL_BUTTON_RIGHT);
            const bool jumpPressed = Input::WasKeyPressed(SDL_SCANCODE_SPACE);

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

            const float uiCameraX = camera.GetX();
            const float uiCameraY = camera.GetY();

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
                    // Order rows: index 0 is REINFORCEMENT, index 1 is
                    // SUPPLY CRATE, matching the labels drawn below.
                    constexpr float rowW = BuyPanelW - 16.0f;
                    constexpr float rowH = 24.0f;

                    for (int i = 0; i < 2; i++)
                    {
                        const float rowX = panelScreenX + 8.0f;
                        const float rowY =
                            panelScreenY + 50.0f + static_cast<float>(i) * 32.0f;

                        if (mouseScreenX < rowX || mouseScreenX > rowX + rowW ||
                            mouseScreenY < rowY || mouseScreenY > rowY + rowH)
                            continue;

                        const int cost =
                            i == 0 ? ReinforcementCost : SupplyCrateCost;

                        if (player.GetGold() < cost)
                            break;

                        player.AddGold(-cost);

                        const bool fromLeft = (rng() % 2u) == 0u;
                        const float shipDir = fromLeft ? 1.0f : -1.0f;
                        const float shipOrigin = fromLeft
                            ? (camera.GetX() - ShipEdgeMargin)
                            : (camera.GetX() + static_cast<float>(ViewWidth) +
                                ShipEdgeMargin);

                        deliveries.Order(
                            i == 0
                                ? DeliveryKind::Reinforcement
                                : DeliveryKind::SupplyCrate,
                            shipOrigin,
                            shipDir,
                            player.GetCenterX());

                        Audio::Play(Sfx::Reload, 0.6f);
                        break;
                    }
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

            // Fixed-timestep simulation.
            accumulator += frameTime;

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

                    if (jumpPressed && player.IsGrounded())
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

                    if (throwPressed && !uiCapturingInput &&
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
                    cameraShake = std::min(cameraShake + 11.0f, 18.0f);
                }

                cameraShake -= cameraShake * 3.5f * FixedTimeStep;

                particles.Update(
                    terrain,
                    FixedTimeStep,
                    actorPtrs,
                    actorCount);

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
                        if (delivery->Kind == DeliveryKind::SupplyCrate)
                        {
                            if (player.IsAlive())
                            {
                                player.Heal(50);
                                player.GetWeapon().Refill();
                            }
                        }
                        else
                        {
                            spawnAlly(delivery->PayloadX);
                        }
                    }
                }

                accumulator -= FixedTimeStep;
            }

            terrain.Update();

            // Camera follows the player, clamped to the world, with
            // explosion shake on top.
            camera.SetPosition(
                std::clamp(
                    player.GetCenterX() - ViewWidth * 0.5f,
                    0.0f,
                    static_cast<float>(worldWidth - ViewWidth)) +
                    unit(rng) * cameraShake,
                std::clamp(
                    player.GetCenterY() - ViewHeight * 0.5f,
                    0.0f,
                    static_cast<float>(worldHeight - ViewHeight)) +
                    unit(rng) * cameraShake);

            // Damage feedback for the vignette.
            if (player.GetHealth() < lastPlayerHealth)
                hurtVignette = 0.6f;

            lastPlayerHealth = player.GetHealth();
            hurtVignette = std::max(0.0f, hurtVignette - frameTime);

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

            grenades.Draw(m_Window);

            deliveries.Draw(m_Window);

            for (Enemy& enemy : enemies)
                enemy.Body->Draw(m_Window);

            for (Ally& ally : allies)
                ally.Body->Draw(m_Window);

            if (player.IsAlive())
                player.Draw(m_Window);

            particles.Draw(m_Window);

            // Crosshair.
            m_Window.DrawFilledRect(
                mouseWorldX - 4.0f, mouseWorldY - 1.0f, 8.0f, 2.0f,
                255, 255, 255, 190);
            m_Window.DrawFilledRect(
                mouseWorldX - 1.0f, mouseWorldY - 4.0f, 2.0f, 8.0f,
                255, 255, 255, 190);

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
                    // Order panel: spend gold mined from the terrain on
                    // a drop-ship delivery.
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

                    const char* names[2] = { "REINFORCEMENT", "SUPPLY CRATE" };
                    const char* subtitles[2] =
                    {
                        "AI RIFLEMAN, FIGHTS FOR YOU",
                        "AMMO REFILL + FIELD MEDKIT",
                    };
                    const int costs[2] = { ReinforcementCost, SupplyCrateCost };

                    for (int i = 0; i < 2; i++)
                    {
                        const float rowY =
                            panelScreenY + 50.0f + static_cast<float>(i) * 32.0f;

                        const bool affordable = player.GetGold() >= costs[i];

                        m_Window.DrawScreenRect(
                            panelScreenX + 8.0f, rowY,
                            BuyPanelW - 16.0f, 24.0f,
                            affordable ? 26 : 20,
                            affordable ? 26 : 20,
                            affordable ? 34 : 24,
                            200);

                        const std::string title =
                            std::string(names[i]) + " - " +
                            std::to_string(costs[i]) + "G";

                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + 14.0f, rowY + 4.0f,
                            2.0f, title,
                            affordable ? 230 : 120,
                            affordable ? 200 : 100,
                            affordable ? 110 : 100);

                        PixelFont::Draw(
                            m_Window,
                            panelScreenX + 14.0f, rowY + 13.0f,
                            2.0f, subtitles[i],
                            affordable ? 150 : 90,
                            affordable ? 152 : 90,
                            affordable ? 160 : 96);
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

            // Backing panel with a subtle border.
            m_Window.DrawScreenRect(8.0f, 8.0f, barWidth + 18.0f, 92.0f,
                120, 126, 148, 60);
            m_Window.DrawScreenRect(9.0f, 9.0f, barWidth + 16.0f, 90.0f,
                12, 12, 18, 190);

            auto drawBar = [&](
                float y,
                float fill,
                Uint8 r, Uint8 g, Uint8 b)
            {
                // Trough.
                m_Window.DrawScreenRect(16.0f, y, barWidth, 9.0f,
                    28, 28, 36, 255);

                if (fill > 0.0f)
                {
                    // Bar with a light sheen on the top half.
                    m_Window.DrawScreenRect(16.0f, y, barWidth * fill, 9.0f,
                        r, g, b, 255);
                    m_Window.DrawScreenRect(16.0f, y, barWidth * fill, 4.0f,
                        255, 255, 255, 46);
                }
            };

            drawBar(14.0f,
                std::max(0, player.GetHealth()) / 100.0f,
                205, 60, 50);

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

                PixelFont::Draw(m_Window, 16.0f, 55.0f, 2.0f, info,
                    235, 235, 240);

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

                PixelFont::Draw(
                    m_Window,
                    16.0f + barWidth -
                        PixelFont::Measure(ammoText, 2.0f),
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
                const float slotX = 16.0f + static_cast<float>(i) * 22.0f;

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
                    16.0f + static_cast<float>(LoadoutSize) * 22.0f + 6.0f;

                m_Window.DrawScreenRect(goldX, 73.0f, 8.0f, 8.0f,
                    24, 22, 27, 255);
                m_Window.DrawScreenRect(goldX + 1.0f, 74.0f, 6.0f, 6.0f,
                    218, 176, 56, 255);
                m_Window.DrawScreenRect(goldX + 1.0f, 74.0f, 6.0f, 2.0f,
                    240, 205, 110, 255);

                PixelFont::Draw(
                    m_Window,
                    goldX + 13.0f,
                    75.0f,
                    2.0f,
                    std::to_string(player.GetGold()),
                    230, 200, 110);
            }

            // Wave status (top right): wave number and enemies left.
            {
                const std::string waveText = "WAVE " + std::to_string(wave);

                PixelFont::Draw(
                    m_Window,
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

            // Bots-spawn toggle button.
            {
                m_Window.DrawScreenRect(
                    BotsButtonX, BotsButtonY, BotsButtonW, BotsButtonH,
                    28, 28, 36, 255);

                m_Window.DrawScreenRect(
                    BotsButtonX + 1.0f, BotsButtonY + 1.0f,
                    BotsButtonW - 2.0f, BotsButtonH - 2.0f,
                    botsEnabled ? 42 : 92,
                    botsEnabled ? 120 : 46,
                    botsEnabled ? 58 : 46,
                    220);

                const std::string botsLabel =
                    botsEnabled ? "BOTS: ON" : "BOTS: OFF";

                PixelFont::Draw(
                    m_Window,
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
