#include "Application.h"

#include "../Actors/Actor.h"
#include "../Actors/AIController.h"
#include "../Audio/Audio.h"
#include "IniFile.h"
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

        struct HealthPickup
        {
            float X;
            float Y;
            float VelY;
        };
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

                    if (Input::IsMouseButtonDown(SDL_BUTTON_LEFT))
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
                        }
                    }

                    if (throwPressed && grenadeCooldown <= 0.0f)
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
                Actor* actorPtrs[MaxEnemies + 1];
                int actorCount = 0;

                if (player.IsAlive())
                    actorPtrs[actorCount++] = &player;

                for (Enemy& enemy : enemies)
                    actorPtrs[actorCount++] = enemy.Body.get();

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
                if (enemies.empty())
                {
                    waveTimer -= FixedTimeStep;

                    if (waveTimer <= 0.0f)
                    {
                        wave++;
                        spawnWave(std::min(1 + wave, MaxEnemies));
                        waveTimer = 4.0f;
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

            for (Enemy& enemy : enemies)
                enemy.Body->Draw(m_Window);

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
