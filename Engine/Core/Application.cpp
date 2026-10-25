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
#include "../Graphics/Texture2D.h"
#include "../Particles/ParticleSystem.h"
#include "../Net/Protocol.h"
#include "../Net/Session.h"
#include "../Sim/World.h"
#include "../UI/Ui.h"
#include "../World/Terrain.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
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

        // Physics runs at a fixed rate (World::FixedTimeStep); rendering
        // runs as fast as it can.
        constexpr float MaxFrameTime = 0.25f;

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

        std::string ReadTextFile(const std::string& path)
        {
            std::size_t size = 0;
            void* data = SDL_LoadFile(path.c_str(), &size);

            if (!data)
                return std::string();

            std::string text(static_cast<const char*>(data), size);
            SDL_free(data);
            return text;
        }

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

    }

    bool LaunchOptions::Parse(int argc, char** argv)
    {
        auto usage = [&]()
        {
            std::cout <<
                "usage: Atlas                    play offline\n"
                "       Atlas --host [port]      host a multiplayer game\n"
                "       Atlas --join <address>[:port]\n"
                "       --survival               endless waves, no bases (offline / host)\n"
                "default port " << DefaultPort << "\n";
            return false;
        };

        auto parsePort = [](const std::string& text, std::uint16_t& out)
        {
            char* end = nullptr;
            const long value = std::strtol(text.c_str(), &end, 10);

            if (!end || *end != '\0' || value <= 0 || value > 65535)
                return false;

            out = static_cast<std::uint16_t>(value);
            return true;
        };

        Port = DefaultPort;

        for (int i = 1; i < argc; i++)
        {
            const std::string arg = argv[i];

            SkipMenu = true;

            if (arg == "--host")
            {
                Mode = LaunchMode::Host;

                if (i + 1 < argc && argv[i + 1][0] != '-')
                {
                    if (!parsePort(argv[++i], Port))
                        return usage();
                }
            }
            else if (arg == "--survival")
            {
                Survival = true;
            }
            else if (arg == "--join" && i + 1 < argc)
            {
                Mode = LaunchMode::Join;
                Address = argv[++i];

                const std::size_t colon = Address.rfind(':');

                if (colon != std::string::npos)
                {
                    if (!parsePort(Address.substr(colon + 1), Port))
                        return usage();

                    Address = Address.substr(0, colon);
                }
            }
            else
            {
                return usage();
            }
        }

        return true;
    }

    Application::Application(const LaunchOptions& options)
        : m_Options(options)
    {
    }

    void Application::Run()
    {
        std::cout << "=================================\n";
        std::cout << "        ATLAS ENGINE\n";
        std::cout << "           v0.6.0\n";
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

        m_Settings.Load();
        ApplySettings();

        MatchRequest request;
        request.Mode = m_Options.Mode;
        request.Address = m_Options.Address;
        request.Port = m_Options.Port;
        request.Survival = m_Options.Survival;

        bool showMenu = !m_Options.SkipMenu;

        for (;;)
        {
            if (showMenu && !RunMenu(request))
                break;

            const MatchExit exit = RunMatch(request);

            if (exit == MatchExit::Quit)
                break;

            showMenu = exit == MatchExit::ToMenu;
        }

        m_Settings.Save();
        Audio::Shutdown();
    }

    void Application::ApplySettings()
    {
        Audio::SetMasterVolume(m_Settings.Volume);
        m_Window.SetFullscreen(m_Settings.Fullscreen);
    }

    bool Application::DrawSettings(Ui& ui, float x, float y)
    {
        const float w = 520.0f;
        const float h = 34.0f;
        const float gap = 42.0f;

        ui.Text(x, y, 3.0f, "SETTINGS", 235, 238, 245);
        y += 40.0f;

        bool changed = false;

        if (ui.Slider(x, y, w, h, "VOLUME", m_Settings.Volume))
            changed = true;
        y += gap;

        int shake = m_Settings.ScreenShake ? 1 : 0;
        if (ui.Choice(x, y, w, h, "SCREEN SHAKE", { "OFF", "ON" }, shake))
        {
            m_Settings.ScreenShake = shake == 1;
            changed = true;
        }
        y += gap;

        int fullscreen = m_Settings.Fullscreen ? 1 : 0;
        if (ui.Choice(x, y, w, h, "FULLSCREEN", { "OFF", "ON" }, fullscreen))
        {
            m_Settings.Fullscreen = fullscreen == 1;
            changed = true;
        }
        y += gap;

        int fps = m_Settings.ShowFps ? 1 : 0;
        if (ui.Choice(x, y, w, h, "SHOW FPS", { "OFF", "ON" }, fps))
        {
            m_Settings.ShowFps = fps == 1;
            changed = true;
        }
        y += gap + 12.0f;

        if (changed)
            ApplySettings();

        if (ui.Button(x, y, 200.0f, 36.0f, "BACK"))
        {
            m_Settings.Save();
            return true;
        }

        return false;
    }

    namespace
    {
        // The menu pointer (the game hides the OS cursor).
        void DrawPointer(Window& window, float x, float y)
        {
            for (int row = 0; row < 12; row++)
            {
                const float width = static_cast<float>(std::min(row, 8)) + 1.0f;
                window.DrawScreenRect(x - 1.0f, y + static_cast<float>(row) - 1.0f,
                    width + 2.0f, 1.0f, 10, 10, 14, 220);
                window.DrawScreenRect(x, y + static_cast<float>(row), width, 1.0f,
                    240, 242, 248, 255);
            }
        }

        // A battle that plays itself behind the main menu: the two sides'
        // AI fight over the brains while the camera drifts after them.
        struct MenuBackdrop
        {
            std::unique_ptr<World> Sim;
            Background Sky;
            Texture2D BodyTexture;
            float Accumulator = 0.0f;
            float Age = 0.0f;
            float CamX = 0.0f;
            float CamY = 0.0f;

            void Restart(SDL_Renderer* renderer, unsigned int seed)
            {
                Sim = std::make_unique<World>();

                WorldConfig config;
                config.Width = 2600;
                config.Height = 1024;
                config.Seed = seed;
                config.Difficulty = 1;
                Sim->Create(config);

                Sim->GetTerrain().CreateTexture(renderer);
                Sky.Create(seed);

                const TeamBase& home = Sim->GetBase(0);

                for (int i = 0; i < 5; i++)
                    Sim->AddAlly(home.X + 180.0f + static_cast<float>(i) * 40.0f);

                Sim->SetAllyOrder(AllyOrder::Attack);

                Accumulator = 0.0f;
                Age = 0.0f;
                CamX = static_cast<float>(config.Width) * 0.5f - ViewWidth * 0.5f;
                CamY = static_cast<float>(Sim->GetTerrain().GetSurfaceY(config.Width / 2)) - ViewHeight * 0.55f;
            }

            void Update(SDL_Renderer* renderer, float dt)
            {
                Age += dt;

                // A fresh battle every couple of minutes, or once decided.
                if (!Sim ||
                    Age > 150.0f ||
                    (Sim->GetOutcome() != MatchOutcome::InProgress && Age > 8.0f &&
                        Sim->GetStats().EndTick + 120u * 6u < Sim->GetTick()))
                {
                    Restart(renderer, static_cast<unsigned int>(SDL_GetPerformanceCounter()));
                }

                Accumulator += std::min(dt, 0.1f);

                while (Accumulator >= World::FixedTimeStep)
                {
                    Sim->Step(nullptr, 0);
                    Accumulator -= World::FixedTimeStep;
                }

                Sim->GetTerrain().Update();

                // Follow the middle of the fighting, slowly.
                float sumX = 0.0f;
                float sumY = 0.0f;
                int count = 0;

                for (const Ally& ally : Sim->GetAllies())
                {
                    sumX += ally.Body->GetCenterX();
                    sumY += ally.Body->GetCenterY();
                    count++;
                }

                for (const Enemy& enemy : Sim->GetEnemies())
                {
                    if (enemy.Attacker)
                    {
                        sumX += enemy.Body->GetCenterX();
                        sumY += enemy.Body->GetCenterY();
                        count++;
                    }
                }

                if (count > 0)
                {
                    const float targetX = sumX / static_cast<float>(count) - ViewWidth * 0.62f;
                    const float targetY = sumY / static_cast<float>(count) - ViewHeight * 0.5f;
                    const float blend = 1.0f - std::exp(-dt * 0.35f);

                    CamX += (targetX - CamX) * blend;
                    CamY += (targetY - CamY) * blend;
                }

                const float maxX = static_cast<float>(Sim->GetTerrain().GetWidth() - ViewWidth);
                const float maxY = static_cast<float>(Sim->GetTerrain().GetHeight() - ViewHeight);
                CamX = std::clamp(CamX, 0.0f, maxX);
                CamY = std::clamp(CamY, 0.0f, maxY);
            }

            void Draw(Window& window, Camera& camera, float time)
            {
                camera.SetPosition(CamX, CamY);
                Sky.Draw(window, CamX, CamY, ViewWidth, ViewHeight, time);
                Sim->GetTerrain().Draw(window);

                auto drawBody = [&](Actor& body)
                {
                    if (!body.GetBodyTexture())
                        body.SetBodyTexture(BodyTexture.GetTexture());

                    if (body.IsAlive())
                        body.Draw(window);
                };

                for (Enemy& enemy : Sim->GetEnemies())
                    drawBody(*enemy.Body);

                for (Ally& ally : Sim->GetAllies())
                    drawBody(*ally.Body);

                Sim->GetDeliveries().Draw(window);
                Sim->GetParticles().Draw(window);
            }
        };
    }

    bool Application::RunMenu(MatchRequest& request)
    {
        enum class Screen { Main, Skirmish, Host, Join, Settings, Controls };

        Screen screen = Screen::Main;

        Camera camera;
        m_Window.SetCamera(&camera);

        Ui ui(m_Window);

        MenuBackdrop backdrop;
        backdrop.BodyTexture.Load(m_Window.GetRenderer(), ResolveAssetPath("Assets/Player.bmp"));
        backdrop.Restart(m_Window.GetRenderer(), static_cast<unsigned int>(SDL_GetPerformanceCounter()));

        // Text fields edit strings; they're parsed when a match starts.
        std::string seedText = request.Seed != 0 ? std::to_string(request.Seed) : std::string();
        std::string portText = std::to_string(request.Port);
        std::string joinText = m_Settings.JoinAddress;

        static const std::vector<std::string> Modes = { "BRAIN ASSAULT", "SURVIVAL" };
        static const std::vector<std::string> Sizes = { "SMALL", "MEDIUM", "LARGE" };
        static const std::vector<std::string> Difficulties = { "EASY", "NORMAL", "HARD" };

        int modeIndex = request.Survival ? 1 : 0;
        std::string error;

        Uint64 last = SDL_GetPerformanceCounter();
        float time = 0.0f;

        for (;;)
        {
            if (!m_Window.PollEvents())
                return false;

            Input::Update();

            const Uint64 now = SDL_GetPerformanceCounter();
            const float dt = std::min(MaxFrameTime, static_cast<float>(
                static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceFrequency())));
            last = now;
            time += dt;

            backdrop.Update(m_Window.GetRenderer(), dt);

            if (Input::WasKeyPressed(SDL_SCANCODE_ESCAPE) && screen != Screen::Main)
            {
                if (screen == Screen::Settings)
                    m_Settings.Save();

                screen = Screen::Main;
                error.clear();
            }

            m_Window.BeginFrame();
            backdrop.Draw(m_Window, camera, time);

            // Darken toward the left where the menu sits.
            for (int band = 0; band < 16; band++)
            {
                const float x = static_cast<float>(band) * 40.0f;
                m_Window.DrawScreenRect(x, 0.0f, 40.0f, static_cast<float>(ViewHeight),
                    6, 7, 12, static_cast<Uint8>(200 - band * 12));
            }

            ui.Begin(time);

            const float left = 80.0f;

            ui.Text(left, 64.0f, 14.0f, "ATLAS", 240, 236, 225);
            ui.Text(left + 4.0f, 150.0f, 2.0f, "DIG. FIGHT. PROTECT THE BRAIN.", 255, 150, 130);

            const float y0 = 220.0f;
            const float bw = 300.0f;
            const float bh = 40.0f;
            const float gap = 50.0f;

            auto startMatch = [&](LaunchMode mode)
            {
                request.Mode = mode;
                request.Survival = modeIndex == 1;

                char* end = nullptr;
                const unsigned long seed = seedText.empty() ? 0ul : std::strtoul(seedText.c_str(), &end, 10);
                request.Seed = static_cast<unsigned int>(seed);

                if (mode == LaunchMode::Host)
                {
                    const long port = std::strtol(portText.c_str(), &end, 10);

                    if (port <= 0 || port > 65535)
                    {
                        error = "BAD PORT";
                        return false;
                    }

                    request.Port = static_cast<std::uint16_t>(port);
                }

                return true;
            };

            bool start = false;

            switch (screen)
            {
            case Screen::Main:
            {
                float y = y0;

                if (ui.Button(left, y, bw, bh, "SKIRMISH")) screen = Screen::Skirmish;
                y += gap;
                if (ui.Button(left, y, bw, bh, "HOST GAME")) screen = Screen::Host;
                y += gap;
                if (ui.Button(left, y, bw, bh, "JOIN GAME")) screen = Screen::Join;
                y += gap;
                if (ui.Button(left, y, bw, bh, "SETTINGS")) screen = Screen::Settings;
                y += gap;
                if (ui.Button(left, y, bw, bh, "CONTROLS")) screen = Screen::Controls;
                y += gap;

                if (ui.Button(left, y, bw, bh, "QUIT"))
                {
                    ui.End();
                    return false;
                }

                break;
            }

            case Screen::Skirmish:
            case Screen::Host:
            {
                const bool host = screen == Screen::Host;
                float y = y0 - 20.0f;
                const float w = 520.0f;
                const float h = 34.0f;

                ui.Text(left, y, 3.0f, host ? "HOST A GAME" : "SKIRMISH", 235, 238, 245);
                y += 40.0f;

                ui.Choice(left, y, w, h, "MODE", Modes, modeIndex);
                y += 42.0f;
                ui.Choice(left, y, w, h, "MAP", Sizes, request.MapSize);
                y += 42.0f;
                ui.Choice(left, y, w, h, "DIFFICULTY", Difficulties, request.Difficulty);
                y += 42.0f;
                ui.TextField(left, y, w, h, "SEED (BLANK = RANDOM)", seedText, 10);
                y += 42.0f;

                if (host)
                {
                    ui.TextField(left, y, w, h, "PORT", portText, 5);
                    y += 42.0f;
                }

                ui.Text(left, y + 4.0f, 1.5f, modeIndex == 0
                    ? "DESTROY THE ENEMY BRAIN. KEEP YOURS ALIVE."
                    : "HOLD OUT AGAINST ENDLESS WAVES.", 170, 176, 190);
                y += 30.0f;

                if (ui.Button(left, y, 240.0f, bh, host ? "HOST" : "START"))
                    start = startMatch(host ? LaunchMode::Host : LaunchMode::Offline);

                if (ui.Button(left + 260.0f, y, 200.0f, bh, "BACK"))
                    screen = Screen::Main;

                break;
            }

            case Screen::Join:
            {
                float y = y0 - 20.0f;

                ui.Text(left, y, 3.0f, "JOIN A GAME", 235, 238, 245);
                y += 40.0f;

                ui.TextField(left, y, 520.0f, 34.0f, "ADDRESS[:PORT]", joinText, 60);
                y += 42.0f;

                ui.Text(left, y + 4.0f, 1.5f,
                    "THE HOST PICKS THE MAP AND MODE. DEFAULT PORT " + std::to_string(DefaultPort) + ".",
                    170, 176, 190);
                y += 30.0f;

                if (ui.Button(left, y, 240.0f, bh, "JOIN") && !joinText.empty())
                {
                    request.Mode = LaunchMode::Join;
                    request.Address = joinText;
                    request.Port = DefaultPort;

                    const std::size_t colon = joinText.rfind(':');

                    if (colon != std::string::npos)
                    {
                        const long port = std::strtol(joinText.c_str() + colon + 1, nullptr, 10);
                        request.Address = joinText.substr(0, colon);

                        if (port > 0 && port <= 65535)
                            request.Port = static_cast<std::uint16_t>(port);
                    }

                    m_Settings.JoinAddress = joinText;
                    m_Settings.Save();
                    start = true;
                }

                if (ui.Button(left + 260.0f, y, 200.0f, bh, "BACK"))
                    screen = Screen::Main;

                break;
            }

            case Screen::Settings:
                if (DrawSettings(ui, left, y0 - 20.0f))
                    screen = Screen::Main;
                break;

            case Screen::Controls:
            {
                static const char* Rows[][2] =
                {
                    { "A / D", "MOVE" },
                    { "SPACE", "JUMP" },
                    { "W / SHIFT", "JETPACK" },
                    { "S", "CROUCH" },
                    { "MOUSE", "AIM" },
                    { "LEFT MOUSE", "FIRE" },
                    { "RIGHT MOUSE", "THROW GRENADE" },
                    { "1-5 / WHEEL", "SMG, SHOTGUN, RIFLE, DIGGER, SHOVEL" },
                    { "R", "RELOAD" },
                    { "TAB (HOLD)", "BUY MENU: ORDER BODIES, WEAPONS, SUPPLIES" },
                    { "Q", "SWITCH TO THE NEXT FRIENDLY BODY" },
                    { "Z / X / C", "SQUAD: DEFEND / FOLLOW / ATTACK" },
                    { "ESC", "PAUSE" },
                };

                float y = y0 - 20.0f;
                ui.Text(left, y, 3.0f, "CONTROLS", 235, 238, 245);
                y += 40.0f;

                for (const auto& row : Rows)
                {
                    ui.Text(left, y, 2.0f, row[0], 255, 236, 170);
                    ui.Text(left + 200.0f, y, 2.0f, row[1], 220, 224, 232);
                    y += 22.0f;
                }

                y += 12.0f;

                if (ui.Button(left, y, 200.0f, bh, "BACK"))
                    screen = Screen::Main;

                break;
            }
            }

            if (!error.empty())
                ui.Text(left, static_cast<float>(ViewHeight) - 60.0f, 2.0f, error, 255, 120, 100);

            ui.Text(static_cast<float>(ViewWidth) - 16.0f, static_cast<float>(ViewHeight) - 20.0f, 1.5f,
                "V0.6.0", 120, 124, 140, 2);

            ui.End();

            DrawPointer(m_Window, Input::GetMouseX(), Input::GetMouseY());
            m_Window.EndFrame();

            if (start)
            {
                m_Window.SetTextInput(false);
                return true;
            }
        }
    }


    MatchExit Application::RunMatch(const MatchRequest& request)
    {
        Camera camera;
        m_Window.SetCamera(&camera);

        // Scene setup is data-driven; Seed 0 means a fresh world each run.
        IniFile sceneIni;
        sceneIni.Load(ResolveAssetPath("Assets/scene.ini"));

        WorldConfig worldConfig;
        worldConfig.Mode = request.Survival ? GameMode::Survival : GameMode::BrainAssault;
        worldConfig.Difficulty = request.Difficulty;

        static constexpr int MapWidths[3] = { DefaultWorldWidth, 3072, 4096 };
        static constexpr int MapHeights[3] = { DefaultWorldHeight, 1152, 1280 };
        const int mapSize = std::clamp(request.MapSize, 0, 2);

        worldConfig.Width = std::max(ViewWidth, MapWidths[mapSize]);
        worldConfig.Height = std::max(ViewHeight, MapHeights[mapSize]);

        unsigned int seed = request.Seed != 0
            ? request.Seed
            : static_cast<unsigned int>(sceneIni.GetInt("World", "Seed", 0));

        if (seed == 0)
            seed = static_cast<unsigned int>(SDL_GetPerformanceCounter());

        worldConfig.Seed = seed;

        // Weapons: data-driven with built-in fallbacks. The text itself
        // is what a host shares, so every peer simulates the same guns.
        const std::string weaponsIni =
            ReadTextFile(ResolveAssetPath("Assets/weapons.ini"));

        // The simulation, run alone, hosted, or joined over the network.
        // Everything below this point is presentation: it turns SDL
        // input into PlayerInput, hands it to the session, and draws
        // what the World reports.
        Session session;

        switch (request.Mode)
        {
        case LaunchMode::Offline:
            session.StartOffline(worldConfig, weaponsIni);
            break;
        case LaunchMode::Host:
            session.StartHost(worldConfig, weaponsIni, request.Port);
            break;
        case LaunchMode::Join:
            session.StartClient(request.Address, request.Port);
            break;
        }

        // Connecting / catching up: a plain status screen until the
        // local player exists (or the attempt fails).
        {
            Uint64 last = SDL_GetPerformanceCounter();
            float failTimer = 0.0f;

            while (session.GetState() != SessionState::Running)
            {
                if (!m_Window.PollEvents())
                    return MatchExit::Quit;

                Input::Update();

                if (Input::WasKeyPressed(SDL_SCANCODE_ESCAPE))
                    return MatchExit::ToMenu;

                const Uint64 now = SDL_GetPerformanceCounter();
                const float dt = std::min(
                    MaxFrameTime,
                    static_cast<float>(
                        static_cast<double>(now - last) /
                        static_cast<double>(SDL_GetPerformanceFrequency())));
                last = now;

                if (session.GetState() == SessionState::Failed)
                {
                    failTimer += dt;

                    if (failTimer > 6.0f)
                        return MatchExit::ToMenu;
                }
                else
                {
                    session.Update(dt, PlayerInput{}, nullptr, nullptr);
                }

                std::string line = session.GetStatusText();

                if (session.GetState() == SessionState::Joining)
                {
                    line += " " + std::to_string(static_cast<int>(
                        session.GetJoinProgress() * 100.0f)) + "%";
                }

                for (char& c : line)
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

                m_Window.BeginFrame();
                m_Window.DrawScreenRect(
                    0.0f, 0.0f,
                    static_cast<float>(ViewWidth), static_cast<float>(ViewHeight),
                    14, 16, 24, 255);
                PixelFont::Draw(
                    m_Window,
                    (static_cast<float>(ViewWidth) - PixelFont::Measure(line, 3.0f)) * 0.5f,
                    static_cast<float>(ViewHeight) * 0.5f - 12.0f,
                    3.0f, line,
                    session.GetState() == SessionState::Failed ? 255 : 230,
                    session.GetState() == SessionState::Failed ? 130 : 230,
                    session.GetState() == SessionState::Failed ? 110 : 235);
                m_Window.EndFrame();
            }

            if (!session.HasWorld())
                return MatchExit::ToMenu;
        }

        World& world = session.GetWorld();

        const int worldWidth = world.GetTerrain().GetWidth();
        const int worldHeight = world.GetTerrain().GetHeight();

        Terrain& terrain = world.GetTerrain();

        if (!terrain.CreateTexture(m_Window.GetRenderer()))
            return MatchExit::ToMenu;

        Background background;
        background.Create(seed);

        constexpr int LoadoutSize = World::LoadoutSize;
        constexpr int MaxAllies = World::MaxAllies;
        constexpr float RespawnTime = 3.0f;

        const WeaponDef* loadout[LoadoutSize];

        for (int i = 0; i < LoadoutSize; i++)
            loadout[i] = world.GetLoadoutWeapon(i);

        const WeaponDef* allyWeapon = world.GetAllyWeapon();

        // One body sprite, shared by every actor (tinted per team).
        Texture2D bodyTexture;

        if (!bodyTexture.Load(
            m_Window.GetRenderer(), ResolveAssetPath("Assets/Player.bmp")))
        {
            std::cout << "Failed to load Player.bmp\n";
        }

        // Players can join mid-game (the slot vector may grow), so the
        // local slot is looked up afresh rather than held by reference;
        // the Actor itself never moves.
        const int localPlayer = session.GetLocalSlot();
        auto localSlot = [&]() -> PlayerSlot& { return world.GetPlayer(localPlayer); };
        Actor& player = *localSlot().Body;

        const ParticleSystem& particles = world.GetParticles();
        const GrenadeSystem& grenades = world.GetGrenades();
        const DeliverySystem& deliveries = world.GetDeliveries();
        std::vector<Enemy>& enemies = world.GetEnemies();
        std::vector<Ally>& allies = world.GetAllies();
        const std::vector<HealthPickup>& pickups = world.GetHealthPickups();
        const std::vector<WeaponPickup>& weaponPickups =
            world.GetWeaponPickups();

        BuyCart cart;

        // Catalog is fixed for the whole run (built from the same
        // WeaponDefs the player's own loadout uses), so it - and the
        // vertical offset of each row within the panel - are computed
        // once rather than every frame.
        std::vector<CatalogEntry> catalog;
        catalog.push_back(
            { CatalogKind::Body, "RIFLEMAN", World::ReinforcementCost, allyWeapon });

        for (int i = 0; i < LoadoutSize; i++)
        {
            std::string name = loadout[i]->Name;

            for (char& c : name)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

            catalog.push_back(
                { CatalogKind::Weapon, name, loadout[i]->Cost, loadout[i] });
        }

        catalog.push_back(
            { CatalogKind::Item, "SUPPLY CRATE", World::SupplyCrateCost, nullptr });

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

        // Cosmetic-only randomness (floating text jitter). Simulation
        // randomness lives in the World and is seeded from the scene.
        std::mt19937 rng(
            static_cast<unsigned int>(SDL_GetPerformanceCounter()));

        // Camera: exponentially-smoothed position with aim lookahead and
        // trauma-based shake (trauma in [0,1]; amplitude scales with its
        // square so small hits barely tremble and big ones really rock).
        float camSmoothX = player.GetCenterX() - ViewWidth * 0.5f;
        float camSmoothY = player.GetCenterY() - ViewHeight * 0.5f;
        float trauma = 0.0f;

        std::vector<Floater> floaters;
        std::uniform_real_distribution<float> unit(-1.0f, 1.0f);

        // Gold gained is batched into one popup instead of spamming a
        // floater for every mined pixel.
        int goldPopupAccum = 0;
        float goldPopupTimer = 0.0f;
        int lastPlayerGold = world.GetTeamGold(0);

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
        bool reloadQueued = false;
        int slotQueued = -1;
        bool switchQueued = false;
        int allyOrderQueued = -1;

        // A brain alarm flashes the HUD for a moment.
        float brainAlarm = 0.0f;

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

        // The Tab-held buy menu.
        bool buyPanelOpen = false;

        // Turns what the simulation reported this tick into sound,
        // camera shake, hit-stop and floating text - judged from the
        // local player's point of view.
        // Center-screen banner with an optional sub-line.
        auto pushBanner = [&](const std::string& banner, const std::string& sub,
            Uint8 r, Uint8 g, Uint8 b, float life = 2.2f)
        {
            floaters.push_back({
                (static_cast<float>(ViewWidth) - PixelFont::Measure(banner, 6.0f)) * 0.5f,
                170.0f, -9.0f, life, life, 6.0f, banner, r, g, b, true });

            if (!sub.empty())
            {
                floaters.push_back({
                    (static_cast<float>(ViewWidth) - PixelFont::Measure(sub, 2.0f)) * 0.5f,
                    212.0f, -9.0f, life, life, 2.0f, sub, 240, 235, 220, true });
            }
        };

        auto handleEvents = [&]()
        {
            for (const SimEvent& event : world.GetEvents())
            {
                switch (event.Type)
                {
                case SimEventType::Sound:
                    Audio::Play(event.Sound, event.Volume);
                    break;

                case SimEventType::GunFired:
                    if (event.Player == localPlayer)
                        crosshairBloom = 1.0f;
                    break;

                case SimEventType::HardLanding:
                    // A camera thump scaled by how hard the fall was.
                    if (event.Player == localPlayer)
                        trauma = std::min(
                            trauma + 0.12f + event.Amount * 0.2f, 1.0f);
                    break;

                case SimEventType::Explosion:
                    trauma = std::min(trauma + 0.5f, 1.0f);
                    break;

                case SimEventType::EnemyKilled:
                {
                    // Kill punch: brief hit-stop plus a camera thump -
                    // but only for kills the player can actually see. An
                    // ally finishing someone off across the map
                    // shouldn't freeze the world for no visible reason.
                    const float killDx = event.X - player.GetCenterX();
                    const float killDy = event.Y - player.GetCenterY();

                    if (std::fabs(killDx) < 720.0f &&
                        std::fabs(killDy) < 440.0f)
                    {
                        hitStop = std::min(hitStop + 0.05f, 0.09f);
                        trauma = std::min(trauma + 0.18f, 1.0f);
                    }
                    break;
                }

                case SimEventType::Damage:
                    floaters.push_back({
                        event.X,
                        event.Y,
                        -34.0f,
                        0.75f, 0.75f,
                        2.0f,
                        std::to_string(event.Value),
                        255, 224, 130,
                        false });
                    break;

                case SimEventType::BrainUnderAttack:
                    if (event.Value == 0)
                    {
                        brainAlarm = 1.0f;
                        pushBanner("BRAIN UNDER ATTACK", "DEFEND YOUR BUNKER", 255, 90, 80, 1.8f);
                    }
                    break;

                case SimEventType::BrainDestroyed:
                    trauma = 1.0f;
                    break;

                case SimEventType::MatchOver:
                    if (event.Value == 0)
                        pushBanner("VICTORY", "THE ENEMY BRAIN IS DESTROYED", 140, 235, 150, 6.0f);
                    else
                        pushBanner("DEFEAT", "YOUR BRAIN IS DESTROYED", 255, 90, 80, 6.0f);
                    break;

                case SimEventType::EnemyDropIncoming:
                    pushBanner("ENEMY DROP", std::to_string(event.Value) + " HOSTILES INBOUND",
                        255, 130, 110);
                    break;

                case SimEventType::BodySwitched:
                    if (event.Player == localPlayer)
                        trauma = std::min(trauma + 0.1f, 1.0f);
                    break;

                case SimEventType::WaveStarted:
                {
                    // Brain assault announces its drops instead.
                    if (world.GetMode() == GameMode::BrainAssault)
                        break;

                    // Center-screen wave banner with a sub-line.
                    const std::string banner =
                        "WAVE " + std::to_string(event.Value);

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
                    break;
                }
                }
            }
        };

        bool running = true;

        // Esc menu, its settings page, and the end-of-match screen.
        Ui ui(m_Window);
        bool paused = false;
        bool pauseSettings = false;
        float resultsTimer = 0.0f;
        bool resultsHidden = false;
        int shownFps = 0;

        bool exitNow = false;
        MatchExit exitTo = MatchExit::ToMenu;

        Uint64 lastCounter = SDL_GetPerformanceCounter();
        const Uint64 frequency = SDL_GetPerformanceFrequency();

        // A buy order waiting for the session to take it.
        bool orderQueued = false;
        OrderRequest queuedOrder;

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
                title << "Atlas - " << frames << " FPS - wave " << world.GetWave()
                    << " - " << particles.GetActiveCount() << " particles";
                m_Window.SetTitle(title.str());

                shownFps = frames;
                fpsTimer = 0.0;
                frames = 0;
            }

            running = m_Window.PollEvents();

            Input::Update();

            if (Input::WasKeyPressed(SDL_SCANCODE_ESCAPE))
            {
                if (pauseSettings)
                    pauseSettings = false;
                else
                    paused = !paused;
            }

            if (world.GetOutcome() != MatchOutcome::InProgress)
                resultsTimer += frameTime;

            const bool resultsShown = resultsTimer > 3.0f && !resultsHidden;

            // A menu is up: the game gets no input (and offline, no time).
            const bool modal = paused || resultsShown;

            // Weapon switching (1-5 or scroll wheel) and reload are
            // edge-triggered per frame and latched for the next tick.
            if (player.IsAlive())
            {
                const int shownSlot =
                    slotQueued >= 0 ? slotQueued : localSlot().CurrentSlot;
                int slot = shownSlot;

                if (Input::WasKeyPressed(SDL_SCANCODE_1)) slot = 0;
                if (Input::WasKeyPressed(SDL_SCANCODE_2)) slot = 1;
                if (Input::WasKeyPressed(SDL_SCANCODE_3)) slot = 2;
                if (Input::WasKeyPressed(SDL_SCANCODE_4)) slot = 3;
                if (Input::WasKeyPressed(SDL_SCANCODE_5)) slot = 4;

                const int wheel = Input::ConsumeWheelSteps();

                if (wheel != 0)
                {
                    slot = (slot - wheel % LoadoutSize + LoadoutSize) %
                        LoadoutSize;
                }

                if (slot != shownSlot)
                    slotQueued = slot;

                if (Input::WasKeyPressed(SDL_SCANCODE_R))
                    reloadQueued = true;
            }

            // Q: jump into the next friendly body. Z / X / C: squad
            // orders (defend the brain / follow me / attack theirs).
            if (Input::WasKeyPressed(SDL_SCANCODE_Q))
                switchQueued = true;

            if (Input::WasKeyPressed(SDL_SCANCODE_Z))
                allyOrderQueued = static_cast<int>(AllyOrder::Defend);

            if (Input::WasKeyPressed(SDL_SCANCODE_X))
                allyOrderQueued = static_cast<int>(AllyOrder::Follow);

            if (Input::WasKeyPressed(SDL_SCANCODE_C))
                allyOrderQueued = static_cast<int>(AllyOrder::Attack);

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
                botsButtonHovered && !modal)
            {
                session.SetBotsEnabled(!world.IsBotsEnabled());
            }

            const bool menuHeld =
                player.IsAlive() && !modal && Input::IsKeyDown(SDL_SCANCODE_TAB);

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

                        if (world.GetTeamGold(0) < cart.GetTotalCost() + entry.Cost)
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
                            world.GetTeamGold(0) >= cart.GetTotalCost())
                        {
                            OrderRequest order;

                            for (const CartBody& body : cart.GetBodies())
                                order.Bodies.push_back(body.Weapon);

                            for (const CartLooseWeapon& loose :
                                cart.GetLooseWeapons())
                            {
                                order.LooseWeapons.push_back(loose.Weapon);
                            }

                            order.SupplyCrates = cart.GetSupplyCrateCount();

                            // The World charges for it on the tick it
                            // lands (on every peer at once, in multiplayer).
                            queuedOrder = order;
                            orderQueued = true;
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

            // This frame's input for the local player.
            PlayerInput input;

            if (Input::IsKeyDown(SDL_SCANCODE_A))
                input.MoveX -= 1.0f;

            if (Input::IsKeyDown(SDL_SCANCODE_D))
                input.MoveX += 1.0f;

            input.Jump = Input::IsKeyDown(SDL_SCANCODE_SPACE);
            input.JumpPressed = jumpQueued;
            input.Jetpack =
                Input::IsKeyDown(SDL_SCANCODE_W) ||
                Input::IsKeyDown(SDL_SCANCODE_LSHIFT);
            input.Crouch = Input::IsKeyDown(SDL_SCANCODE_S);

            input.AimX = mouseWorldX;
            input.AimY = mouseWorldY;

            // No shooting or grenade-throwing through the UI.
            input.Fire =
                Input::IsMouseButtonDown(SDL_BUTTON_LEFT) &&
                !uiCapturingInput;
            input.ThrowPressed = throwQueued && !uiCapturingInput;
            input.ReloadPressed = reloadQueued;
            input.SelectSlot = static_cast<std::int8_t>(slotQueued);
            input.SwitchBodyPressed = switchQueued;
            input.AllyOrder = static_cast<std::int8_t>(allyOrderQueued);

            if (modal)
            {
                const float aimX = input.AimX;
                const float aimY = input.AimY;
                input = PlayerInput{};
                input.AimX = aimX;
                input.AimY = aimY;
                orderQueued = false;
            }

            // Fixed-timestep simulation, run by the session. Hit-stop
            // holds back the frame's sim time, freezing the world for a
            // beat while rendering carries on.
            float simTime = frameTime;

            if (paused && request.Mode == LaunchMode::Offline)
                simTime = 0.0f;

            if (hitStop > 0.0f)
            {
                hitStop -= frameTime;
                simTime = 0.0f;
            }

            const bool inputTaken = session.Update(
                simTime,
                input,
                orderQueued ? &queuedOrder : nullptr,
                handleEvents);

            // Latched input edges are consumed once the session has
            // actually taken them; a frame with zero ticks (hit-stop)
            // keeps them queued for the next real tick.
            if (inputTaken)
            {
                throwQueued = false;
                jumpQueued = false;
                reloadQueued = false;
                slotQueued = -1;
                orderQueued = false;
                switchQueued = false;
                allyOrderQueued = -1;
            }

            // Gold popups, batched: while gold keeps arriving the window
            // extends; once it goes quiet the total pops up.
            {
                const int gold = world.GetTeamGold(0);

                if (gold > lastPlayerGold)
                {
                    goldPopupAccum += gold - lastPlayerGold;
                    goldPopupTimer = 0.4f;
                }

                lastPlayerGold = gold;

                if (goldPopupTimer > 0.0f)
                {
                    goldPopupTimer -= frameTime;

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

                const float amp = m_Settings.ScreenShake ? trauma * trauma * 26.0f : 0.0f;

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
                        pickup.X - camera.GetX() -
                            PixelFont::Measure(name, 1.5f) * 0.5f,
                        pickup.Y - camera.GetY() - 18.0f,
                        1.5f, name, 210, 225, 235);
                }
            }

            grenades.Draw(m_Window);

            deliveries.Draw(m_Window);

            // Actors spawned by the World borrow the client's shared body
            // sprite the first time they are drawn.
            for (Enemy& enemy : enemies)
            {
                if (!enemy.Body->GetBodyTexture())
                    enemy.Body->SetBodyTexture(bodyTexture.GetTexture());
            }

            for (Ally& ally : allies)
            {
                if (!ally.Body->GetBodyTexture())
                    ally.Body->SetBodyTexture(bodyTexture.GetTexture());
            }

            for (int slot = 0; slot < world.GetPlayerCount(); slot++)
            {
                Actor& body = *world.GetPlayer(slot).Body;

                if (!body.GetBodyTexture())
                    body.SetBodyTexture(bodyTexture.GetTexture());
            }

            for (Enemy& enemy : enemies)
                enemy.Body->Draw(m_Window);

            for (Ally& ally : allies)
                ally.Body->Draw(m_Window);

            // Every player, the local one last so it stays on top.
            for (int slot = 0; slot < world.GetPlayerCount(); slot++)
            {
                Actor& body = *world.GetPlayer(slot).Body;

                if (slot != localPlayer && body.IsAlive())
                    body.Draw(m_Window);
            }

            if (player.IsAlive())
                player.Draw(m_Window);

            // Name tags over the other players.
            for (int slot = 0; slot < world.GetPlayerCount(); slot++)
            {
                const Actor& body = *world.GetPlayer(slot).Body;

                if (slot == localPlayer || !body.IsAlive())
                    continue;

                const std::string tag = "P" + std::to_string(slot + 1);

                PixelFont::Draw(
                    m_Window,
                    body.GetCenterX() - camera.GetX() -
                        PixelFont::Measure(tag, 1.5f) * 0.5f,
                    body.GetY() - camera.GetY() - 16.0f,
                    1.5f, tag, 235, 235, 240, 220);
            }

            // Which body is yours: once there are several on your side,
            // a small bobbing chevron over it.
            if (player.IsAlive() && !allies.empty())
            {
                const float bob = std::sin(timeSeconds * 5.0f) * 2.0f;
                const float cx = player.GetCenterX() - camera.GetX();
                const float top = player.GetY() - camera.GetY() - 18.0f + bob;

                for (int row = 0; row < 4; row++)
                {
                    m_Window.DrawScreenRect(
                        cx - 4.0f + static_cast<float>(row), top + static_cast<float>(row),
                        8.0f - static_cast<float>(row) * 2.0f, 1.0f,
                        140, 230, 255, 230);
                }
            }

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
                        "GOLD: " + std::to_string(world.GetTeamGold(0));

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
                            world.GetTeamGold(0) >= cart.GetTotalCost() + entry.Cost;

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
                            world.GetTeamGold(0) >= cart.GetTotalCost();

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
                const bool selected = i == localSlot().CurrentSlot;
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
                    std::to_string(world.GetTeamGold(0)),
                    230, 200, 110);
            }

            // Wave status (top right): wave number and enemies left.
            {
                const std::string waveText = "WAVE " + std::to_string(world.GetWave());

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
                    static_cast<Uint8>((world.IsBotsEnabled() ? 42 : 92) + hoverBoost),
                    static_cast<Uint8>((world.IsBotsEnabled() ? 120 : 46) + hoverBoost),
                    static_cast<Uint8>((world.IsBotsEnabled() ? 58 : 46) + hoverBoost),
                    220);

                const std::string botsLabel =
                    world.IsBotsEnabled() ? "BOTS: ON" : "BOTS: OFF";

                drawText(
                    BotsButtonX +
                        (BotsButtonW - PixelFont::Measure(botsLabel, 2.0f)) *
                            0.5f,
                    BotsButtonY + 7.0f,
                    2.0f,
                    botsLabel,
                    235, 235, 240);
            }

            // Multiplayer status under the bots button.
            if (session.GetRole() != SessionRole::Offline)
            {
                std::string status;
                Uint8 r = 200, g = 220, b = 235;

                if (session.GetState() == SessionState::Failed)
                {
                    status = session.GetStatusText();
                    r = 255; g = 130; b = 110;
                }
                else if (session.HasDesynced())
                {
                    status = session.GetStatusText();
                    r = 255; g = 180; b = 90;
                }
                else if (session.GetRole() == SessionRole::Host)
                {
                    status = "HOST :" + std::to_string(request.Port) + " - " +
                        std::to_string(world.GetPlayerCount()) + " PLAYERS";
                }
                else
                {
                    status = "ONLINE - PLAYER " + std::to_string(localPlayer + 1);
                }

                for (char& c : status)
                    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

                PixelFont::Draw(
                    m_Window,
                    static_cast<float>(ViewWidth) - 20.0f -
                        PixelFont::Measure(status, 1.5f),
                    BotsButtonY + BotsButtonH + 8.0f,
                    1.5f, status, r, g, b);
            }

            // --- Brain assault: both brains, the next enemy drop, squad
            // orders, and arrows toward each brain when it's off screen ---
            if (world.GetMode() == GameMode::BrainAssault)
            {
                brainAlarm = std::max(0.0f, brainAlarm - frameTime * 0.6f);

                const float panelW = 560.0f;
                const float panelX = (static_cast<float>(ViewWidth) - panelW) * 0.5f;
                const float panelY = 8.0f;

                const bool flash = brainAlarm > 0.0f &&
                    std::fmod(timeSeconds, 0.4f) < 0.2f;

                m_Window.DrawScreenRect(panelX - 1.0f, panelY - 1.0f, panelW + 2.0f, 52.0f,
                    flash ? 200 : 120, flash ? 60 : 126, flash ? 60 : 148, flash ? 160 : 60);
                m_Window.DrawScreenRect(panelX, panelY, panelW, 50.0f, 12, 12, 18, 195);

                auto brainBar = [&](float x, const std::string& label, const TeamBase& base,
                    Uint8 r, Uint8 g, Uint8 b)
                {
                    const float w = 180.0f;
                    const float health = base.GetBrainHealth();

                    drawText(x, panelY + 7.0f, 1.5f, label, r, g, b);
                    m_Window.DrawScreenRect(x - 1.0f, panelY + 19.0f, w + 2.0f, 11.0f, 52, 54, 66, 255);
                    m_Window.DrawScreenRect(x, panelY + 20.0f, w, 9.0f, 24, 24, 32, 255);

                    if (health > 0.0f)
                    {
                        m_Window.DrawScreenRect(x, panelY + 20.0f, w * health, 9.0f, r, g, b, 255);
                        m_Window.DrawScreenRect(x, panelY + 20.0f, w * health, 4.0f, 255, 255, 255, 46);
                    }
                };

                brainBar(panelX + 12.0f, "YOUR BRAIN", world.GetBase(0), 120, 200, 255);
                brainBar(panelX + panelW - 192.0f, "ENEMY BRAIN", world.GetBase(1), 255, 118, 106);

                // Next drop countdown, centered between the bars.
                if (world.GetOutcome() == MatchOutcome::InProgress && world.IsBotsEnabled())
                {
                    const int secs = static_cast<int>(std::ceil(world.GetWaveTimer()));
                    const std::string drop = "DROP " + std::to_string(secs);

                    drawText(
                        panelX + (panelW - PixelFont::Measure(drop, 1.5f)) * 0.5f,
                        panelY + 21.0f, 1.5f, drop, 255, 170, 150);
                }

                // Squad orders, the current one lit.
                {
                    static const char* Names[] = { "Z DEFEND", "X FOLLOW", "C ATTACK" };
                    const int current = static_cast<int>(world.GetAllyOrder());

                    float x = panelX + 12.0f;

                    drawText(x, panelY + 36.0f, 1.5f,
                        "SQUAD " + std::to_string(allies.size()), 200, 205, 215);
                    x += 92.0f;

                    for (int i = 0; i < 3; i++)
                    {
                        const bool lit = i == current;
                        drawText(x, panelY + 36.0f, 1.5f, Names[i],
                            lit ? 140 : 110, lit ? 230 : 112, lit ? 255 : 124);
                        x += 84.0f;
                    }

                    if (!allies.empty())
                        drawText(x, panelY + 36.0f, 1.5f, "Q SWITCH", 200, 205, 215);
                }

                // Off-screen arrows toward each living brain.
                for (int team = 0; team < World::TeamCount; team++)
                {
                    const TeamBase& base = world.GetBase(team);

                    if (!base.BrainAlive)
                        continue;

                    const float sx = base.GetBrainCenterX() - camera.GetX();
                    const float sy = base.GetBrainCenterY() - camera.GetY();

                    if (sx >= 0.0f && sx < static_cast<float>(ViewWidth) &&
                        sy >= 0.0f && sy < static_cast<float>(ViewHeight))
                        continue;

                    const float ax = std::clamp(sx, 26.0f, static_cast<float>(ViewWidth) - 26.0f);
                    const float ay = std::clamp(sy, 90.0f, static_cast<float>(ViewHeight) - 26.0f);
                    const float dir = sx < 0.0f ? -1.0f : 1.0f;

                    const Uint8 r = team == 0 ? 120 : 255;
                    const Uint8 g = team == 0 ? 200 : 118;
                    const Uint8 b = team == 0 ? 255 : 106;

                    for (int col = 0; col < 7; col++)
                    {
                        const float h = 14.0f - static_cast<float>(col) * 2.0f;
                        m_Window.DrawScreenRect(
                            ax + dir * static_cast<float>(col), ay - h * 0.5f, 1.0f, h, r, g, b, 230);
                    }

                    const float distance = std::fabs(base.GetBrainCenterX() - player.GetCenterX());
                    const std::string label = std::to_string(static_cast<int>(distance / 10.0f)) + "M";

                    drawText(
                        ax - dir * 12.0f - (dir > 0.0f ? PixelFont::Measure(label, 1.5f) : 0.0f),
                        ay - 4.0f, 1.5f, label, r, g, b);
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
                        1.0f - localSlot().RespawnTimer / RespawnTime, 0.0f, 1.0f),
                    12.0f,
                    240, 240, 240, 220);
            }

            if (m_Settings.ShowFps)
            {
                PixelFont::Draw(m_Window, 12.0f, static_cast<float>(ViewHeight) - 18.0f, 1.5f,
                    std::to_string(shownFps) + " FPS", 200, 210, 220);
            }

            // --- Pause menu / end-of-match screen ---
            if (modal)
            {
                ui.Begin(timeSeconds);

                m_Window.DrawScreenRect(0.0f, 0.0f,
                    static_cast<float>(ViewWidth), static_cast<float>(ViewHeight), 4, 5, 10, 150);

                const float panelW = 600.0f;
                const float panelX = (static_cast<float>(ViewWidth) - panelW) * 0.5f;
                const bool offline = request.Mode == LaunchMode::Offline;

                if (pauseSettings)
                {
                    ui.Panel(panelX, 150.0f, panelW, 330.0f);

                    if (DrawSettings(ui, panelX + 40.0f, 180.0f))
                        pauseSettings = false;
                }
                else if (paused)
                {
                    ui.Panel(panelX, 160.0f, panelW, 360.0f);
                    ui.Text(static_cast<float>(ViewWidth) * 0.5f, 190.0f, 5.0f, "PAUSED", 240, 236, 225, 1);

                    if (!offline)
                    {
                        ui.Text(static_cast<float>(ViewWidth) * 0.5f, 225.0f, 1.5f,
                            "THE MATCH GOES ON WITHOUT YOU", 255, 170, 150, 1);
                    }

                    const float bx = static_cast<float>(ViewWidth) * 0.5f - 150.0f;
                    float by = 250.0f;

                    if (ui.Button(bx, by, 300.0f, 38.0f, "RESUME"))
                        paused = false;
                    by += 46.0f;

                    if (ui.Button(bx, by, 300.0f, 38.0f, "SETTINGS"))
                        pauseSettings = true;
                    by += 46.0f;

                    if (ui.Button(bx, by, 300.0f, 38.0f, "RESTART", offline))
                    {
                        exitNow = true;
                        exitTo = MatchExit::Restart;
                    }
                    by += 46.0f;

                    if (ui.Button(bx, by, 300.0f, 38.0f, "MAIN MENU"))
                    {
                        exitNow = true;
                        exitTo = MatchExit::ToMenu;
                    }
                    by += 46.0f;

                    if (ui.Button(bx, by, 300.0f, 38.0f, "QUIT GAME"))
                    {
                        exitNow = true;
                        exitTo = MatchExit::Quit;
                    }
                }
                else
                {
                    const bool won = world.GetOutcome() == MatchOutcome::Victory;
                    const MatchStats& stats = world.GetStats();

                    ui.Panel(panelX, 140.0f, panelW, 400.0f);
                    ui.Text(static_cast<float>(ViewWidth) * 0.5f, 170.0f, 8.0f,
                        won ? "VICTORY" : "DEFEAT",
                        won ? 140 : 255, won ? 235 : 90, won ? 150 : 80, 1);
                    ui.Text(static_cast<float>(ViewWidth) * 0.5f, 220.0f, 2.0f,
                        won ? "THE ENEMY BRAIN IS DESTROYED" : "YOUR BRAIN IS DESTROYED",
                        220, 224, 232, 1);

                    const int seconds = static_cast<int>(stats.EndTick / 120u);
                    const std::string clock = std::to_string(seconds / 60) + ":" +
                        (seconds % 60 < 10 ? "0" : "") + std::to_string(seconds % 60);

                    const std::pair<std::string, std::string> rows[] =
                    {
                        { "MATCH TIME", clock },
                        { "ENEMIES KILLED", std::to_string(stats.EnemiesKilled) },
                        { "BODIES LOST", std::to_string(stats.BodiesLost) },
                        { "BODIES ORDERED", std::to_string(stats.BodiesOrdered) },
                        { "GOLD MINED", std::to_string(stats.GoldMined) },
                    };

                    float ry = 262.0f;

                    for (const auto& row : rows)
                    {
                        ui.Text(panelX + 140.0f, ry, 2.0f, row.first, 170, 176, 190);
                        ui.Text(panelX + panelW - 140.0f, ry, 2.0f, row.second, 255, 236, 170, 2);
                        ry += 26.0f;
                    }

                    const float by = 410.0f;

                    if (offline && ui.Button(panelX + 40.0f, by, 160.0f, 38.0f, "PLAY AGAIN"))
                    {
                        exitNow = true;
                        exitTo = MatchExit::Restart;
                    }

                    if (ui.Button(panelX + 220.0f, by, 160.0f, 38.0f, "MAIN MENU"))
                    {
                        exitNow = true;
                        exitTo = MatchExit::ToMenu;
                    }

                    if (ui.Button(panelX + 400.0f, by, 160.0f, 38.0f, "WATCH"))
                        resultsHidden = true;

                    ui.Text(static_cast<float>(ViewWidth) * 0.5f, by + 60.0f, 1.5f,
                        "ESC FOR THE MENU", 140, 144, 160, 1);
                }

                ui.End();

                // The menu pointer over the crosshair.
                for (int row = 0; row < 12; row++)
                {
                    const float width = static_cast<float>(std::min(row, 8)) + 1.0f;
                    m_Window.DrawScreenRect(Input::GetMouseX() - 1.0f, Input::GetMouseY() + static_cast<float>(row) - 1.0f,
                        width + 2.0f, 1.0f, 10, 10, 14, 220);
                    m_Window.DrawScreenRect(Input::GetMouseX(), Input::GetMouseY() + static_cast<float>(row),
                        width, 1.0f, 240, 242, 248, 255);
                }
            }

            m_Window.EndFrame();

            if (exitNow)
                return exitTo;
        }

        return MatchExit::Quit;
    }
}

