#include "Application.h"

#include "../Actors/Actor.h"
#include "../Actors/AIController.h"
#include "../Audio/Audio.h"
#include "../Combat/Grenade.h"
#include "../Combat/Weapon.h"
#include "../Graphics/Camera.h"
#include "../Input/Input.h"
#include "../Particles/ParticleSystem.h"
#include "../World/Terrain.h"

#include <SDL3/SDL.h>

#include <algorithm>
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

        constexpr int WorldWidth = 2048;
        constexpr int WorldHeight = 1024;

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
            if (def.Kind == WeaponKind::Digger)
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
    }

    Application::Application()
    {
    }

    void Application::Run()
    {
        std::cout << "=================================\n";
        std::cout << "        ATLAS ENGINE\n";
        std::cout << "           v0.4.0\n";
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

        Terrain terrain;

        if (!terrain.Create(
            m_Window.GetRenderer(),
            WorldWidth,
            WorldHeight))
        {
            return;
        }

        // Weapons: data-driven with built-in fallbacks.
        const std::vector<WeaponDef> weaponDefs =
            LoadWeaponDefs(ResolveAssetPath("Assets/weapons.ini"));

        const WeaponDef* loadout[] =
        {
            FindWeaponDef(weaponDefs, "SMG"),
            FindWeaponDef(weaponDefs, "Shotgun"),
            FindWeaponDef(weaponDefs, "Rifle"),
            FindWeaponDef(weaponDefs, "Digger"),
        };

        const WeaponDef* enemyWeapon = FindWeaponDef(weaponDefs, "EnemyGun");

        const std::string playerSprite = ResolveAssetPath("Assets/Player.bmp");

        Actor player;

        if (!player.LoadBodySprite(m_Window.GetRenderer(), playerSprite))
        {
            std::cout << "Failed to load Player.bmp\n";
        }

        player.SetTeam(0);
        player.SetWeaponDef(loadout[0]);
        player.Spawn(terrain, WorldWidth * 0.5f);

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
        bool playerGibbed = false;
        bool playerWasGrounded = true;
        float playerPrevFallSpeed = 0.0f;

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
                150.0f, static_cast<float>(WorldWidth - 150));

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

            // Weapon switching and reload are edge-triggered per frame.
            if (player.IsAlive())
            {
                if (Input::WasKeyPressed(SDL_SCANCODE_1)) currentWeapon = 0;
                if (Input::WasKeyPressed(SDL_SCANCODE_2)) currentWeapon = 1;
                if (Input::WasKeyPressed(SDL_SCANCODE_3)) currentWeapon = 2;
                if (Input::WasKeyPressed(SDL_SCANCODE_4)) currentWeapon = 3;

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
                        jet);

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

                            Audio::Play(
                                FireSfxFor(*player.GetWeapon().GetDef()),
                                0.8f);
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
                        player.Spawn(terrain, WorldWidth * 0.5f);
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
                    Audio::Play(Sfx::Explosion);

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

                        enemies.erase(enemies.begin() +
                            static_cast<std::ptrdiff_t>(i));
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

            // Camera follows the player, clamped to the world.
            camera.SetPosition(
                std::clamp(
                    player.GetCenterX() - ViewWidth * 0.5f,
                    0.0f,
                    static_cast<float>(WorldWidth - ViewWidth)),
                std::clamp(
                    player.GetCenterY() - ViewHeight * 0.5f,
                    0.0f,
                    static_cast<float>(WorldHeight - ViewHeight)));

            m_Window.BeginFrame();

            terrain.Draw(m_Window);

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

            // --- HUD ---
            const float barWidth = 190.0f;

            // Health.
            m_Window.DrawScreenRect(14.0f, 14.0f, barWidth + 4.0f, 12.0f,
                20, 20, 24, 200);
            m_Window.DrawScreenRect(16.0f, 16.0f,
                barWidth * std::max(0, player.GetHealth()) / 100.0f, 8.0f,
                205, 60, 50, 255);

            // Jetpack fuel.
            m_Window.DrawScreenRect(14.0f, 30.0f, barWidth + 4.0f, 10.0f,
                20, 20, 24, 200);
            m_Window.DrawScreenRect(16.0f, 32.0f,
                barWidth * player.GetFuel(), 6.0f,
                90, 150, 230, 255);

            // Ammo / reload.
            m_Window.DrawScreenRect(14.0f, 44.0f, barWidth + 4.0f, 10.0f,
                20, 20, 24, 200);

            const Weapon& weapon = player.GetWeapon();

            if (weapon.IsReloading())
            {
                m_Window.DrawScreenRect(16.0f, 46.0f,
                    barWidth * weapon.GetReloadProgress(), 6.0f,
                    230, 150, 60, 255);
            }
            else if (weapon.GetClipSize() > 0)
            {
                m_Window.DrawScreenRect(16.0f, 46.0f,
                    barWidth * weapon.GetAmmo() /
                        static_cast<float>(weapon.GetClipSize()),
                    6.0f,
                    230, 210, 90, 255);
            }
            else
            {
                m_Window.DrawScreenRect(16.0f, 46.0f, barWidth, 6.0f,
                    150, 150, 150, 255);
            }

            // Weapon slots.
            for (int i = 0; i < 4; i++)
            {
                const bool selected = i == currentWeapon;

                m_Window.DrawScreenRect(
                    14.0f + static_cast<float>(i) * 18.0f,
                    58.0f,
                    14.0f,
                    8.0f,
                    selected ? 240 : 70,
                    selected ? 240 : 70,
                    selected ? 240 : 80,
                    220);
            }

            // Wave pips (top right): filled per enemy still alive.
            for (std::size_t i = 0; i < enemies.size(); i++)
            {
                m_Window.DrawScreenRect(
                    static_cast<float>(ViewWidth) - 22.0f -
                        static_cast<float>(i) * 14.0f,
                    16.0f,
                    10.0f,
                    10.0f,
                    255, 118, 106, 255);
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
