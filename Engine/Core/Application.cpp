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
#include "../Graphics/MapThumbnail.h"
#include "../Graphics/PixelFont.h"
#include "../Graphics/SpriteBank.h"
#include "../Input/Input.h"
#include "../Graphics/Texture2D.h"
#include "../Particles/ParticleSystem.h"
#include "../Net/Protocol.h"
#include "../Net/Session.h"
#include "../Sim/World.h"
#include "../UI/BuyMenu.h"
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

        // Lights the world drawn so far: open sky is a cool moonlit
        // ambient, underground fades to a dim cave ambient, molten rock
        // glows, and the given body carries a soft lamp so the player is
        // never lost in the dark. Lights from glows (muzzle flashes,
        // fire, explosions) were collected while drawing.
        void LightScene(Window& window, const Terrain& terrain,
            float cameraX, float cameraY, const Actor* lampHolder)
        {
            if (!window.IsLightingEnabled())
                return;

            constexpr float ColumnWidth = 8.0f;
            constexpr int Columns = ViewWidth / 8 + 4;

            const float firstX = std::floor(cameraX / ColumnWidth) * ColumnWidth - ColumnWidth * 2.0f;
            float surface[Columns];

            for (int i = 0; i < Columns; i++)
            {
                const int x = std::clamp(
                    static_cast<int>(firstX + (static_cast<float>(i) + 0.5f) * ColumnWidth),
                    0, terrain.GetWidth() - 1);

                surface[i] = static_cast<float>(terrain.GetSurfaceY(x));
            }

            // Skylight spills sideways a little under overhangs and
            // roofs: take the highest opening nearby, then soften, so a
            // bunker wall or a shaft doesn't cut a hard edge in the light.
            {
                float spread[Columns];

                for (int i = 0; i < Columns; i++)
                {
                    float best = surface[i];

                    for (int k = -3; k <= 3; k++)
                    {
                        const int j = std::clamp(i + k, 0, Columns - 1);
                        best = std::min(best, surface[j] + static_cast<float>(std::abs(k)) * 18.0f);
                    }

                    spread[i] = best;
                }

                for (int i = 0; i < Columns; i++)
                {
                    float sum = 0.0f;

                    for (int k = -2; k <= 2; k++)
                        sum += spread[std::clamp(i + k, 0, Columns - 1)];

                    surface[i] = sum / 5.0f + 6.0f;
                }
            }

            // Molten rock lights its surroundings; sampled coarsely.
            constexpr int LavaStep = 10;
            const int x0 = std::max(0, static_cast<int>(cameraX) - 40);
            const int y0 = std::max(0, static_cast<int>(cameraY) - 40);
            const int x1 = std::min(terrain.GetWidth(), static_cast<int>(cameraX) + ViewWidth + 40);
            const int y1 = std::min(terrain.GetHeight(), static_cast<int>(cameraY) + ViewHeight + 40);

            for (int y = y0 - y0 % LavaStep; y < y1; y += LavaStep)
            {
                for (int x = x0 - x0 % LavaStep; x < x1; x += LavaStep)
                {
                    if (terrain.GetMaterial(x, y) == Material::Lava)
                    {
                        window.AddLight(static_cast<float>(x), static_cast<float>(y),
                            46.0f, 255, 120, 40, 0.45f);
                    }
                }
            }

            if (lampHolder && lampHolder->IsAlive())
            {
                window.AddLight(lampHolder->GetCenterX(), lampHolder->GetCenterY() - 4.0f,
                    190.0f, 255, 236, 205, 0.55f);
            }

            // Each biome's sky casts its own color over everything.
            static constexpr SDL_Color SkyAmbient[static_cast<int>(Biome::Count)] =
            {
                { 196, 198, 226, 255 }, // temperate: cool moonlight
                { 220, 196, 190, 255 }, // desert: warm afterglow
                { 204, 210, 226, 255 }, // arctic: cold and clear
                { 218, 166, 154, 255 }, // volcanic: fire-lit smoke
            };

            static constexpr SDL_Color CaveAmbient[static_cast<int>(Biome::Count)] =
            {
                { 74, 76, 100, 255 },
                { 86, 72, 72, 255 },
                { 78, 82, 102, 255 },
                { 90, 60, 56, 255 },
            };

            const int biome = std::clamp(static_cast<int>(terrain.GetBiome()), 0, static_cast<int>(Biome::Count) - 1);

            AmbientLight ambient;
            ambient.SkyAmbient = SkyAmbient[biome];
            ambient.CaveAmbient = CaveAmbient[biome];
            ambient.SurfaceY = surface;
            ambient.Columns = Columns;
            ambient.ColumnWidth = ColumnWidth;
            ambient.FirstColumnX = firstX;
            ambient.Fade = 140.0f;

            window.ApplyLighting(ambient);
        }

        // Bots-spawn toggle button (screen space, top right).
        constexpr float BotsButtonW = 108.0f;
        constexpr float BotsButtonH = 20.0f;
        constexpr float BotsButtonX =
            static_cast<float>(ViewWidth) - 20.0f - BotsButtonW;
        constexpr float BotsButtonY = 54.0f;

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
        std::cout << "           v0.12.0\n";
        std::cout << "=================================\n\n";

        if (!m_Window.Create("Atlas", ViewWidth, ViewHeight))
        {
            return;
        }

        if (!Audio::Init())
        {
            std::cout << "(no audio device - running silent)\n";
        }

        {
            const std::string firstSound = ResolveAssetPath("Assets/Sounds/uiclick.wav");
            const int sounds = Audio::LoadSoundPack(
                firstSound.substr(0, firstSound.find_last_of("/\\")));

            if (sounds > 0)
                std::cout << "Loaded " << sounds << " sound takes\n";
        }

        {
            std::string spritesIni = ResolveAssetPath("Assets/Sprites/sprites.ini");
            const int sprites = SpriteBank::Load(
                m_Window.GetRenderer(),
                spritesIni.substr(0, spritesIni.find_last_of("/\\")));

            std::cout << "Loaded " << sprites << " sprites\n";
        }

        {
            const std::string regular = ResolveAssetPath("Assets/Fonts/ChakraPetch-SemiBold.ttf");
            const std::string bold = ResolveAssetPath("Assets/Fonts/ChakraPetch-Bold.ttf");

            if (!PixelFont::LoadFaces(m_Window.GetRenderer(), regular, bold))
                std::cout << "(fonts missing - using the pixel font)\n";
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

            // Clear any blast flash the match left behind.
            m_Window.SetScreenTint(0, 0, 0, 0);
            ApplySettings();

            if (exit == MatchExit::Quit)
                break;

            showMenu = exit == MatchExit::ToMenu;

            // "Play again" on a random map rolls a new one.
            request.RandomSeed = 0;
        }

        m_Settings.Save();
        SpriteBank::Unload();
        Audio::Shutdown();
    }

    void Application::ApplySettings()
    {
        Audio::SetMasterVolume(m_Settings.Volume);
        Audio::SetMusicVolume(m_Settings.MusicVolume);
        m_Window.SetFullscreen(m_Settings.Fullscreen);
        m_Window.SetLightingEnabled(m_Settings.Lighting);
        m_Window.SetBloom(m_Settings.Bloom ? 1.0f : 0.0f);
        m_Window.SetZoom(m_Settings.GetZoom());
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

        if (ui.Slider(x, y, w, h, "MUSIC", m_Settings.MusicVolume))
            changed = true;
        y += gap;

        int shake = m_Settings.ScreenShake;
        if (ui.Choice(x, y, w, h, "SCREEN SHAKE", { "OFF", "SUBTLE", "STRONG" }, shake))
        {
            m_Settings.ScreenShake = shake;
            changed = true;
        }
        y += gap;

        int lighting = m_Settings.Lighting ? 1 : 0;
        if (ui.Choice(x, y, w, h, "LIGHTING", { "OFF", "ON" }, lighting))
        {
            m_Settings.Lighting = lighting == 1;
            changed = true;
        }
        y += gap;

        int bloom = m_Settings.Bloom ? 1 : 0;
        if (ui.Choice(x, y, w, h, "BLOOM", { "OFF", "ON" }, bloom))
        {
            m_Settings.Bloom = bloom == 1;
            changed = true;
        }
        y += gap;

        int zoom = m_Settings.Zoom;
        if (ui.Choice(x, y, w, h, "WORLD ZOOM", { "1X", "1.25X", "1.5X", "2X" }, zoom))
        {
            m_Settings.Zoom = zoom;
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
                config.Biome = ResolveBiome(0, seed);
                Sim->Create(config);

                Sim->GetTerrain().CreateTexture(renderer);
                Sky.Create(seed, config.Biome);

                const TeamBase& home = Sim->GetBase(0);

                for (int i = 0; i < 5; i++)
                    Sim->AddAlly(home.X + 180.0f + static_cast<float>(i) * 40.0f);

                Sim->SetAllyOrder(AllyOrder::Attack);

                Accumulator = 0.0f;
                Age = 0.0f;
                CamX = static_cast<float>(config.Width) * 0.5f - ViewWidth * 0.5f;
                CamY = static_cast<float>(Sim->GetTerrain().GetSurfaceY(config.Width / 2)) - ViewHeight * 0.55f;
            }

            void Update(SDL_Renderer* renderer, float dt, float viewW, float viewH)
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
                    const float targetX = sumX / static_cast<float>(count) - viewW * 0.62f;
                    const float targetY = sumY / static_cast<float>(count) - viewH * 0.5f;
                    const float blend = 1.0f - std::exp(-dt * 0.35f);

                    CamX += (targetX - CamX) * blend;
                    CamY += (targetY - CamY) * blend;
                }

                const float maxX = static_cast<float>(Sim->GetTerrain().GetWidth()) - viewW;
                const float maxY = static_cast<float>(Sim->GetTerrain().GetHeight()) - viewH;
                CamX = std::clamp(CamX, 0.0f, maxX);
                CamY = std::clamp(CamY, 0.0f, maxY);
            }

            void Draw(Window& window, Camera& camera, float time)
            {
                camera.SetPosition(CamX, CamY);
                window.BeginWorld();
                Sky.Draw(window, CamX, CamY,
                    static_cast<int>(window.GetViewWidth()) + 2,
                    static_cast<int>(window.GetViewHeight()) + 2, time);
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

                LightScene(window, Sim->GetTerrain(), camera.GetX(), camera.GetY(), nullptr);
                window.EndWorld();
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
        static const std::vector<std::string> Biomes = { "RANDOM", "TEMPERATE", "DESERT", "ARCTIC", "VOLCANIC" };

        int modeIndex = request.Survival ? 1 : 0;
        std::string error;

        // Setup-screen map preview. A blank seed means "random", but the
        // random seed is rolled here so the preview shows the real map.
        MapThumbnail preview;
        std::unique_ptr<Terrain> previewTerrain;
        unsigned int randomSeed = static_cast<unsigned int>(SDL_GetPerformanceCounter() % 999983u) + 1u;
        unsigned int previewSeed = 0;
        int previewSize = -1;
        Biome previewBiome = Biome::Count;
        std::string lastSeedText = seedText;
        float seedTextStill = 1.0f;

        auto effectiveSeed = [&]()
        {
            if (seedText.empty())
                return randomSeed;

            const unsigned long typed = std::strtoul(seedText.c_str(), nullptr, 10);
            return typed != 0ul ? static_cast<unsigned int>(typed) : randomSeed;
        };

        Uint64 last = SDL_GetPerformanceCounter();
        float time = 0.0f;

        for (;;)
        {
            if (!m_Window.PollEvents())
                return false;

            Input::Update();
            Audio::UpdateMusic(0.2f);

            const Uint64 now = SDL_GetPerformanceCounter();
            const float dt = std::min(MaxFrameTime, static_cast<float>(
                static_cast<double>(now - last) / static_cast<double>(SDL_GetPerformanceFrequency())));
            last = now;
            time += dt;

            backdrop.Update(m_Window.GetRenderer(), dt, m_Window.GetViewWidth(), m_Window.GetViewHeight());

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
                request.RandomSeed = effectiveSeed();

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
                ui.Choice(left, y, w, h, "TERRAIN", Biomes, request.Biome);
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
                y += 18.0f;
                ui.Text(left, y + 4.0f, 1.5f,
                    GetBiomeBlurb(ResolveBiome(request.Biome, effectiveSeed())), 140, 150, 168);
                y += 30.0f;

                // --- Map preview and landing zone ---
                {
                    if (seedText != lastSeedText)
                    {
                        lastSeedText = seedText;
                        seedTextStill = 0.0f;
                    }

                    seedTextStill += dt;

                    static constexpr int PreviewWidths[3] = { DefaultWorldWidth, 3072, 4096 };
                    static constexpr int PreviewHeights[3] = { DefaultWorldHeight, 1152, 1280 };
                    const int size = std::clamp(request.MapSize, 0, 2);

                    // Rebuild once typing has paused.
                    const Biome biome = ResolveBiome(request.Biome, effectiveSeed());

                    if ((effectiveSeed() != previewSeed || size != previewSize || biome != previewBiome) &&
                        seedTextStill > 0.35f)
                    {
                        previewSeed = effectiveSeed();
                        previewSize = size;
                        previewBiome = biome;
                        previewTerrain = std::make_unique<Terrain>();
                        previewTerrain->Create(PreviewWidths[size], PreviewHeights[size], previewSeed, biome);

                        const int thumbHeight = 520 * PreviewHeights[size] / PreviewWidths[size];
                        preview.Update(m_Window.GetRenderer(), *previewTerrain, 520, thumbHeight);
                    }

                    const float px = 680.0f;
                    const float py = y0 + 20.0f;
                    const float pw = static_cast<float>(preview.GetWidth());
                    const float ph = static_cast<float>(preview.GetHeight());

                    // Backing panel so the title and hints read over the menu backdrop.
                    m_Window.DrawScreenRect(px - 14.0f, y0 - 32.0f, pw + 28.0f, ph + 102.0f, 10, 12, 20, 205);

                    ui.Text(px, y0 - 20.0f, 2.0f, (seedText.empty()
                        ? "RANDOM MAP (SEED " + std::to_string(randomSeed) + ")"
                        : std::string("MAP PREVIEW")) + "  " + GetBiomeName(previewBiome < Biome::Count ? previewBiome : biome),
                        220, 224, 235);

                    m_Window.DrawScreenRect(px - 3.0f, py - 3.0f, pw + 6.0f, ph + 6.0f, 60, 64, 80, 255);
                    preview.Draw(m_Window.GetRenderer(), px, py);

                    if (modeIndex == 0 && previewTerrain && pw > 0.0f)
                    {
                        // Where you may land, and where you will.
                        const float zoneLeft = px + pw * 0.06f;
                        const float zoneRight = px + pw * 0.42f;
                        m_Window.DrawScreenRect(zoneLeft, py, zoneRight - zoneLeft, ph, 90, 160, 255, 30);

                        const float mx = Input::GetMouseX();
                        const float my = Input::GetMouseY();
                        const bool overMap = mx >= px && mx <= px + pw && my >= py && my <= py + ph;

                        if (overMap && Input::WasMouseButtonPressed(SDL_BUTTON_LEFT))
                        {
                            request.LandingZone = std::clamp(
                                static_cast<int>(std::lround((mx - px) / pw * 100.0f)), 6, 42);
                            Audio::Play(Sfx::UiClick, 0.5f);
                        }

                        auto marker = [&](float fraction, Uint8 r, Uint8 g, Uint8 b, const std::string& label)
                        {
                            const int worldX = static_cast<int>(fraction * static_cast<float>(previewTerrain->GetWidth()));
                            const float surface = static_cast<float>(previewTerrain->GetSurfaceY(worldX)) /
                                static_cast<float>(previewTerrain->GetHeight());
                            const float mxp = px + fraction * pw;
                            const float myp = py + surface * ph;

                            m_Window.DrawScreenRect(mxp - 5.0f, myp - 8.0f, 10.0f, 7.0f, 24, 22, 27, 255);
                            m_Window.DrawScreenRect(mxp - 4.0f, myp - 7.0f, 8.0f, 5.0f, r, g, b, 255);
                            m_Window.DrawScreenRect(mxp - 0.5f, myp - 24.0f, 1.0f, 16.0f, r, g, b, 200);
                            PixelFont::Draw(m_Window, mxp - PixelFont::Measure(label, 1.5f) * 0.5f, myp - 36.0f,
                                1.5f, label, r, g, b);
                        };

                        marker(static_cast<float>(request.LandingZone) / 100.0f, 110, 180, 255, "YOU");
                        marker(0.88f, 255, 110, 90, "ENEMY");

                        ui.Text(px, py + ph + 12.0f, 1.5f,
                            "CLICK THE MAP TO CHOOSE YOUR LANDING ZONE.", 170, 176, 190);
                        ui.Text(px, py + ph + 28.0f, 1.5f,
                            "FURTHER EAST IS CLOSER TO THE FIGHT.", 130, 136, 150);
                    }
                }

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
                    { "1-6 / WHEEL", "SWITCH BETWEEN THE ITEMS THIS BODY CARRIES" },
                    { "R", "RELOAD" },
                    { "TAB / B", "BUY MENU: ORDER BODIES, WEAPONS, CRAFTS, PRESETS" },
                    { "Q", "SWITCH TO THE NEXT FRIENDLY BODY" },
                    { "Z / X / C", "SQUAD: DEFEND / FOLLOW / ATTACK" },
                    { "M", "TOGGLE THE MINIMAP" },
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
                "V0.12.0", 120, 124, 140, 2);

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
        worldConfig.LandingZone = request.LandingZone;

        static constexpr int MapWidths[3] = { DefaultWorldWidth, 3072, 4096 };
        static constexpr int MapHeights[3] = { DefaultWorldHeight, 1152, 1280 };
        const int mapSize = std::clamp(request.MapSize, 0, 2);

        worldConfig.Width = std::max(ViewWidth, MapWidths[mapSize]);
        worldConfig.Height = std::max(ViewHeight, MapHeights[mapSize]);

        unsigned int seed = request.Seed != 0
            ? request.Seed
            : request.RandomSeed != 0
                ? request.RandomSeed
                : static_cast<unsigned int>(sceneIni.GetInt("World", "Seed", 0));

        if (seed == 0)
            seed = static_cast<unsigned int>(SDL_GetPerformanceCounter());

        worldConfig.Seed = seed;
        worldConfig.Biome = ResolveBiome(request.Biome, seed);

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
        background.Create(seed, terrain.GetBiome());

        constexpr float RespawnTime = 3.0f;

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

        // The Cortex Command-style order screen (Tab / B).
        BuyMenu buyMenu(world, allyWeapon);

        // Held after the buy menu takes a click, so the button still
        // being down doesn't open fire once it closes.
        bool fireLock = false;

        auto makeBuyContext = [&]()
        {
            BuyMenuContext context;
            context.Gold = world.GetTeamGold(0);
            context.AlliesFielded = static_cast<int>(allies.size());

            for (const Drone& drone : world.GetDrones())
                context.DronesFielded += drone.Team == 0 ? 1 : 0;

            context.LocalPlayer = localPlayer;
            return context;
        };

        // Cosmetic-only randomness (floating text jitter). Simulation
        // randomness lives in the World and is seeded from the scene.
        std::mt19937 rng(
            static_cast<unsigned int>(SDL_GetPerformanceCounter()));

        // Camera: exponentially-smoothed position with aim lookahead and
        // trauma-based shake (trauma in [0,1]; amplitude scales with its
        // square so small hits barely tremble and big ones really rock).
        float camSmoothX = player.GetCenterX() - m_Window.GetViewWidth() * 0.5f;
        float camSmoothY = player.GetCenterY() - m_Window.GetViewHeight() * 0.5f;
        float trauma = 0.0f;
        float camLeadX = 0.0f;
        float camLeadY = 0.0f;

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

        // Health bar keeps a decaying "ghost" of recent damage.
        float healthGhost = 1.0f;

        float timeSeconds = 0.0f;
        float hurtVignette = 0.0f;

        // A nearby blast washes the screen with warm light and swells the
        // bloom for a moment.
        float blastFlash = 0.0f;
        int lastPlayerHealth = player.GetHealth();

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

        // How much fighting is going on near the view, for the music.
        float combatHeat = 0.0f;

        // Minimap (M toggles), refreshed from the terrain twice a second.
        MapThumbnail minimap;
        float minimapTimer = 0.0f;
        bool showMinimap = true;

        auto handleEvents = [&]()
        {
            const float listenX = camera.GetX() + m_Window.GetViewWidth() * 0.5f;
            const float listenY = camera.GetY() + m_Window.GetViewHeight() * 0.5f;

            for (const SimEvent& event : world.GetEvents())
            {
                switch (event.Type)
                {
                case SimEventType::Sound:
                {
                    if (!event.Positional)
                    {
                        Audio::Play(event.Sound, event.Volume);
                        break;
                    }

                    // Full volume across the screen, fading out over
                    // about another screen beyond it; panned by side.
                    const float dx = event.X - listenX;
                    const float dy = event.Y - listenY;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    const float fade = std::clamp(1.0f - (distance - 560.0f) / 1100.0f, 0.0f, 1.0f);

                    if (fade > 0.02f)
                    {
                        Audio::Play(event.Sound, event.Volume * fade,
                            std::clamp(dx / 900.0f, -1.0f, 1.0f));
                    }

                    const bool fighting =
                        event.Sound == Sfx::Shot || event.Sound == Sfx::Shotgun ||
                        event.Sound == Sfx::Rifle || event.Sound == Sfx::Sniper ||
                        event.Sound == Sfx::Rocket || event.Sound == Sfx::Flame ||
                        event.Sound == Sfx::Explosion;

                    if (fighting && fade > 0.0f)
                        combatHeat = std::min(1.5f, combatHeat + 0.05f * fade +
                            (event.Sound == Sfx::Explosion ? 0.3f * fade : 0.0f));
                    break;
                }

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
                {
                    // Shake by how close it was.
                    const float dx = event.X - listenX;
                    const float dy = event.Y - listenY;
                    const float near = std::clamp(1.0f - std::sqrt(dx * dx + dy * dy) / 1200.0f, 0.0f, 1.0f);
                    trauma = std::min(trauma + 0.5f * near, 1.0f);
                    blastFlash = std::max(blastFlash, near * near * 0.8f);
                    break;
                }

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
                        Audio::Play(Sfx::Alarm, 0.7f);
                    }
                    break;

                case SimEventType::BrainDestroyed:
                    trauma = 1.0f;
                    break;

                case SimEventType::MatchOver:
                    if (event.Value == 0)
                    {
                        pushBanner("VICTORY", "THE ENEMY BRAIN IS DESTROYED", 140, 235, 150, 6.0f);
                        Audio::Play(Sfx::Victory, 0.8f);
                    }
                    else
                    {
                        pushBanner("DEFEAT", "YOUR BRAIN IS DESTROYED", 255, 90, 80, 6.0f);
                        Audio::Play(Sfx::Defeat, 0.8f);
                    }
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
                else if (buyMenu.IsActive() && !paused)
                    buyMenu.Close();
                else
                    paused = !paused;
            }

            if (world.GetOutcome() != MatchOutcome::InProgress)
                resultsTimer += frameTime;

            const bool resultsShown = resultsTimer > 3.0f && !resultsHidden;

            // A menu is up: the game gets no input (and offline, no time).
            const bool modal = paused || resultsShown;

            if (!modal)
                buyMenu.HandleToggleKeys();

            // Item switching (1-6 or scroll wheel) and reload are
            // edge-triggered per frame and latched for the next tick.
            // The buy menu has the number keys and wheel while it's up.
            if (player.IsAlive() && !buyMenu.IsActive())
            {
                const int shownSlot =
                    slotQueued >= 0 ? slotQueued : player.GetSelectedItem();
                const int itemCount = std::max(1, player.GetItemCount());
                int slot = shownSlot;

                if (Input::WasKeyPressed(SDL_SCANCODE_1)) slot = 0;
                if (Input::WasKeyPressed(SDL_SCANCODE_2)) slot = 1;
                if (Input::WasKeyPressed(SDL_SCANCODE_3)) slot = 2;
                if (Input::WasKeyPressed(SDL_SCANCODE_4)) slot = 3;
                if (Input::WasKeyPressed(SDL_SCANCODE_5)) slot = 4;
                if (Input::WasKeyPressed(SDL_SCANCODE_6)) slot = 5;

                if (slot >= itemCount)
                    slot = shownSlot;

                const int wheel = Input::ConsumeWheelSteps();

                if (wheel != 0)
                {
                    slot = (slot - wheel % itemCount + itemCount) %
                        itemCount;
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

            if (Input::WasKeyPressed(SDL_SCANCODE_M))
                showMinimap = !showMinimap;

            if (Input::WasKeyPressed(SDL_SCANCODE_Z))
                allyOrderQueued = static_cast<int>(AllyOrder::Defend);

            if (Input::WasKeyPressed(SDL_SCANCODE_X))
                allyOrderQueued = static_cast<int>(AllyOrder::Follow);

            // (C cycles the craft while the buy menu is up.)
            if (Input::WasKeyPressed(SDL_SCANCODE_C) && !buyMenu.IsActive())
                allyOrderQueued = static_cast<int>(AllyOrder::Attack);

            const float mouseWorldX = m_Window.ScreenToWorldX(Input::GetMouseX());
            const float mouseWorldY = m_Window.ScreenToWorldY(Input::GetMouseY());

            throwQueued = throwQueued ||
                Input::WasMouseButtonPressed(SDL_BUTTON_RIGHT);
            jumpQueued = jumpQueued ||
                Input::WasKeyPressed(SDL_SCANCODE_SPACE);

            // --- UI: bots-spawn toggle button ---
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
                botsButtonHovered && !modal && !buyMenu.IsActive())
            {
                session.SetBotsEnabled(!world.IsBotsEnabled());
            }

            // While the buy menu is open, or the mouse is over a HUD
            // button, the world shouldn't respond to left/right clicks
            // (no shooting or grenade-throwing through the UI). Movement
            // is left untouched so the player can still walk around with
            // the menu open.
            if (buyMenu.IsActive())
                fireLock = true;
            else if (!Input::IsMouseButtonDown(SDL_BUTTON_LEFT))
                fireLock = false;

            const bool uiCapturingInput = buyMenu.IsActive() || fireLock || botsButtonHovered;

            // An order the menu confirmed last frame goes to the session.
            if (OrderRequest order; buyMenu.TakeOrder(order))
            {
                // The World charges for it on the tick it lands (on every
                // peer at once, in multiplayer).
                queuedOrder = order;
                orderQueued = true;
            }

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
            // The target leads toward the cursor so the view shows more
            // of what matters. The lead comes from the cursor's SCREEN
            // offset, never the aim direction: aim is measured in world
            // space through the camera, so leading by it fed the camera's
            // own motion back into its target and made it hunt and
            // wobble. Exponential smoothing eases every move; shake is
            // smooth multi-frequency noise scaled by trauma^2, and trauma
            // drains away so a hit is a thump, not a permanent tremor.
            {
                float lookX = 0.0f;
                float lookY = 0.0f;

                if (player.IsAlive() && !modal)
                {
                    const float offX = std::clamp(
                        (mouseScreenX - ViewWidth * 0.5f) / (ViewWidth * 0.5f), -1.0f, 1.0f);
                    const float offY = std::clamp(
                        (mouseScreenY - ViewHeight * 0.5f) / (ViewHeight * 0.5f), -1.0f, 1.0f);

                    // A small dead zone keeps the view still while aiming
                    // near the body.
                    auto shape = [](float v)
                    {
                        const float a = std::max(0.0f, std::fabs(v) - 0.15f) / 0.85f;
                        return std::copysign(a * a * (3.0f - 2.0f * a), v);
                    };

                    lookX = shape(offX) * 150.0f;
                    lookY = shape(offY) * 80.0f;
                }

                // The lead eases on its own, slower than the follow, so
                // flicking the mouse pans instead of jerking.
                const float leadBlend = 1.0f - std::exp(-frameTime * 3.0f);
                camLeadX += (lookX - camLeadX) * leadBlend;
                camLeadY += (lookY - camLeadY) * leadBlend;

                const float viewW = m_Window.GetViewWidth();
                const float viewH = m_Window.GetViewHeight();
                const float maxCamX = std::max(0.0f, static_cast<float>(worldWidth) - viewW);
                const float maxCamY = std::max(0.0f, static_cast<float>(worldHeight) - viewH);

                const float targetX = std::clamp(
                    player.GetCenterX() + camLeadX - viewW * 0.5f,
                    0.0f, maxCamX);
                const float targetY = std::clamp(
                    player.GetCenterY() + camLeadY - viewH * 0.5f,
                    0.0f, maxCamY);

                // A respawn far away snaps instead of panning across the
                // whole map.
                if (std::fabs(targetX - camSmoothX) > 620.0f ||
                    std::fabs(targetY - camSmoothY) > 480.0f)
                {
                    camSmoothX = targetX;
                    camSmoothY = targetY;
                }

                const float blend =
                    1.0f - std::exp(-frameTime * 8.0f);

                camSmoothX += (targetX - camSmoothX) * blend;
                camSmoothY += (targetY - camSmoothY) * blend;

                trauma = std::max(0.0f, trauma - frameTime * 1.4f);

                const float maxShake =
                    m_Settings.ScreenShake == 2 ? 12.0f :
                    m_Settings.ScreenShake == 1 ? 5.0f : 0.0f;
                const float amp = trauma * trauma * maxShake;

                const float shakeX = amp *
                    (0.6f * std::sin(timeSeconds * 31.0f) +
                     0.4f * std::sin(timeSeconds * 17.7f + 1.3f));
                const float shakeY = amp *
                    (0.6f * std::sin(timeSeconds * 27.3f + 2.1f) +
                     0.4f * std::sin(timeSeconds * 14.1f));

                // Sub-pixel camera: the world is drawn on a whole-pixel
                // grid (so nothing shimmers) and the finished image is
                // slid by the remainder when it is scaled to the screen.
                // Shake is sized in screen pixels, whatever the zoom.
                const float zoom = m_Window.GetZoom();

                camera.SetPosition(
                    std::clamp(camSmoothX + shakeX / zoom, 0.0f, maxCamX),
                    std::clamp(camSmoothY + shakeY / zoom, 0.0f, maxCamY));
            }

            // Damage feedback: vignette pulse plus a camera thump.
            if (player.GetHealth() < lastPlayerHealth)
            {
                // Scaled by the hit, so a burning tick or a stray pellet
                // barely nudges and a rocket really lands.
                const float lost = static_cast<float>(lastPlayerHealth - player.GetHealth());
                hurtVignette = std::min(0.6f, 0.2f + lost * 0.015f);
                trauma = std::min(trauma + std::min(0.35f, lost * 0.012f), 1.0f);
            }

            lastPlayerHealth = player.GetHealth();
            hurtVignette = std::max(0.0f, hurtVignette - frameTime);

            blastFlash = std::max(0.0f, blastFlash - frameTime * 2.5f);
            m_Window.SetScreenTint(255, 170, 90, static_cast<Uint8>(blastFlash * blastFlash * 70.0f));
            m_Window.SetBloom(m_Settings.Bloom ? 1.0f + blastFlash : 0.0f);

            // Visual-decay state for the HUD and crosshair.
            crosshairBloom = std::max(0.0f, crosshairBloom - frameTime * 6.0f);

            combatHeat = std::max(0.0f, combatHeat - frameTime * 0.12f);
            Audio::UpdateMusic(std::min(1.0f, combatHeat * 1.4f));

            {
                const float healthFill =
                    std::max(0, player.GetHealth()) /
                        static_cast<float>(player.GetMaxHealth());

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
            m_Window.BeginWorld();

            background.Draw(
                m_Window,
                camera.GetX(),
                camera.GetY(),
                static_cast<int>(m_Window.GetViewWidth()) + 2,
                static_cast<int>(m_Window.GetViewHeight()) + 2,
                timeSeconds);

            terrain.Draw(m_Window);

            // Medkits.
            for (const HealthPickup& pickup : pickups)
            {
                if (const SpriteFrame* sprite = SpriteBank::Find("medkit"))
                {
                    m_Window.DrawSprite(*sprite, pickup.X, pickup.Y + 4.0f);
                    continue;
                }

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

                if (const SpriteFrame* sprite = SpriteBank::Find("crate_weapon"))
                {
                    m_Window.DrawSprite(*sprite, pickup.X, pickup.Y + 2.0f);
                }
                else
                {
                    m_Window.DrawFilledRect(
                        pickup.X - 6.0f, pickup.Y - 6.0f, 12.0f, 8.0f,
                        24, 22, 27, 255);
                    m_Window.DrawFilledRect(
                        pickup.X - 5.0f, pickup.Y - 5.0f, 10.0f, 6.0f,
                        72, 82, 56, 255);
                    m_Window.DrawFilledRect(
                        pickup.X - 5.0f, pickup.Y - 5.0f, 10.0f, 1.5f,
                        92, 104, 70, 255);
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

            // Drones: a little gun pod under two rotors, a team light.
            for (const Drone& drone : world.GetDrones())
            {
                const float x = std::floor(drone.X);
                const float y = std::floor(drone.Y + std::sin(drone.Phase * 9.0f) * 0.5f);
                const bool hurt = drone.HurtFlash > 0.0f;
                const Uint8 shell = hurt ? 230 : 84;

                if (const SpriteFrame* sprite =
                    SpriteBank::Find(drone.Team == 0 ? "drone_team0" : "drone_team1"))
                {
                    // Spinning blades over the two rotor housings.
                    const float blade = 3.0f + std::fabs(std::sin(drone.Phase * 40.0f)) * 5.0f;

                    for (int side = -1; side <= 1; side += 2)
                    {
                        const float rx = x + static_cast<float>(side) * 6.5f;
                        m_Window.DrawFilledRect(rx - blade, y - 7.0f, blade * 2.0f, 1.0f,
                            200, 205, 215, 150);
                    }

                    m_Window.DrawSprite(*sprite, x, y, 0.0f, drone.AimX < 0.0f, false, 1.0f,
                        hurt ? 200 : 0);

                    for (int seg = 0; seg < 3; seg++)
                    {
                        const float t = 3.0f + static_cast<float>(seg) * 2.0f;
                        m_Window.DrawFilledRect(
                            x + drone.AimX * t - 1.0f, y + 4.0f + drone.AimY * t - 1.0f,
                            2.0f, 2.0f, 40, 40, 48, 255);
                    }

                    if (drone.Firing && drone.Cooldown > 1.0f / 6.0f - 0.04f)
                    {
                        m_Window.DrawGlow(x + drone.AimX * 10.0f, y + 4.0f + drone.AimY * 10.0f,
                            10.0f, 255, 200, 100, 80);
                    }

                    m_Window.DrawGlow(x, y, 9.0f,
                        drone.Team == 0 ? 90 : 230, drone.Team == 0 ? 150 : 70,
                        drone.Team == 0 ? 240 : 60, 30);
                    continue;
                }

                // Rotor arms and spinning blades (two blur widths).
                m_Window.DrawFilledRect(x - 11.0f, y - 5.0f, 22.0f, 2.0f, 34, 34, 40, 255);

                const float blade = 4.0f + std::fabs(std::sin(drone.Phase * 40.0f)) * 5.0f;

                for (int side = -1; side <= 1; side += 2)
                {
                    const float rx = x + static_cast<float>(side) * 10.0f;
                    m_Window.DrawFilledRect(rx - 1.0f, y - 8.0f, 2.0f, 3.0f, 40, 40, 46, 255);
                    m_Window.DrawFilledRect(rx - blade, y - 9.0f, blade * 2.0f, 1.0f, 200, 205, 215, 140);
                }

                // Body.
                m_Window.DrawFilledRect(x - 7.0f, y - 4.0f, 14.0f, 9.0f, 24, 22, 27, 255);
                m_Window.DrawFilledRect(x - 6.0f, y - 3.0f, 12.0f, 7.0f, shell,
                    static_cast<Uint8>(hurt ? 230 : 90), static_cast<Uint8>(hurt ? 230 : 102), 255);
                m_Window.DrawFilledRect(x - 6.0f, y - 3.0f, 12.0f, 2.0f,
                    static_cast<Uint8>(hurt ? 255 : 118), static_cast<Uint8>(hurt ? 255 : 124),
                    static_cast<Uint8>(hurt ? 255 : 138), 255);

                // Team stripe and eye.
                const Uint8 teamR = drone.Team == 0 ? 90 : 230;
                const Uint8 teamG = drone.Team == 0 ? 150 : 70;
                const Uint8 teamB = drone.Team == 0 ? 240 : 60;

                m_Window.DrawFilledRect(x - 6.0f, y + 2.0f, 12.0f, 1.0f, teamR, teamG, teamB, 255);
                m_Window.DrawFilledRect(x + (drone.AimX < 0.0f ? -5.0f : 2.0f), y - 1.0f, 3.0f, 2.0f,
                    teamR, teamG, teamB, 255);
                m_Window.DrawGlow(x, y, 9.0f, teamR, teamG, teamB, 40);

                // Gun under the belly, along the aim.
                for (int seg = 0; seg < 4; seg++)
                {
                    const float t = 3.0f + static_cast<float>(seg) * 2.0f;
                    m_Window.DrawFilledRect(
                        x + drone.AimX * t - 1.0f, y + 4.0f + drone.AimY * t - 1.0f,
                        2.0f, 2.0f, 40, 40, 48, 255);
                }

                if (drone.Firing && drone.Cooldown > 1.0f / 6.0f - 0.04f)
                {
                    m_Window.DrawGlow(x + drone.AimX * 12.0f, y + 4.0f + drone.AimY * 12.0f,
                        10.0f, 255, 200, 100, 80);
                }
            }

            // Laser sights on player-held sniper rifles.
            for (int slot = 0; slot < world.GetPlayerCount(); slot++)
            {
                Actor& body = *world.GetPlayer(slot).Body;
                const WeaponDef* def = body.GetWeapon().GetDef();

                if (!body.IsAlive() || body.IsArmDestroyed() || !def || !def->LaserSight)
                    continue;

                const float startX = body.GetMuzzleX();
                const float startY = body.GetMuzzleY();
                const float range = 1400.0f;

                float endX = startX + body.GetAimDirX() * range;
                float endY = startY + body.GetAimDirY() * range;
                float hitX = 0.0f;
                float hitY = 0.0f;
                const bool hit = terrain.RaycastSolid(
                    startX, startY, body.GetAimDirX(), body.GetAimDirY(), range, hitX, hitY);

                if (hit)
                {
                    endX = hitX;
                    endY = hitY;
                }

                const float length = std::sqrt(
                    (endX - startX) * (endX - startX) + (endY - startY) * (endY - startY));
                const int dots = static_cast<int>(length / 2.0f);

                for (int d = 1; d < dots; d++)
                {
                    const float t = static_cast<float>(d) / static_cast<float>(dots);

                    m_Window.DrawFilledRect(
                        startX + (endX - startX) * t, startY + (endY - startY) * t,
                        1.0f, 1.0f, 255, 60, 50, static_cast<Uint8>(230.0f * (1.0f - t * 0.5f)));
                }

                if (hit)
                {
                    m_Window.DrawGlow(endX, endY, 6.0f, 255, 60, 40, 140);
                    m_Window.DrawFilledRect(endX - 1.0f, endY - 1.0f, 2.0f, 2.0f, 255, 140, 120, 255);
                }
            }

            // Every player, the local one last so it stays on top.
            for (int slot = 0; slot < world.GetPlayerCount(); slot++)
            {
                Actor& body = *world.GetPlayer(slot).Body;

                if (slot != localPlayer && body.IsAlive())
                    body.Draw(m_Window);
            }

            if (player.IsAlive())
                player.Draw(m_Window);

            particles.Draw(m_Window);

            LightScene(m_Window, terrain, camera.GetX(), camera.GetY(), &player);

            m_Window.EndWorld();

            // Weapon pickups' names float over their cases.
            for (const WeaponPickup& pickup : weaponPickups)
            {
                if (!pickup.Weapon)
                    continue;

                std::string name = pickup.Weapon->Name;

                for (char& c : name)
                    c = static_cast<char>(std::toupper(
                        static_cast<unsigned char>(c)));

                PixelFont::Draw(
                    m_Window,
                    m_Window.WorldToScreenX(pickup.X) -
                        PixelFont::Measure(name, 1.5f) * 0.5f,
                    m_Window.WorldToScreenY(pickup.Y - 12.0f) - 14.0f,
                    1.5f, name, 210, 225, 235);
            }

            // Name tags over the other players.
            for (int slot = 0; slot < world.GetPlayerCount(); slot++)
            {
                const Actor& body = *world.GetPlayer(slot).Body;

                if (slot == localPlayer || !body.IsAlive())
                    continue;

                const std::string tag = "P" + std::to_string(slot + 1);

                PixelFont::Draw(
                    m_Window,
                    m_Window.WorldToScreenX(body.GetCenterX()) -
                        PixelFont::Measure(tag, 1.5f) * 0.5f,
                    m_Window.WorldToScreenY(body.GetY()) - 20.0f,
                    1.5f, tag, 235, 235, 240, 220);
            }

            // Which body is yours: once there are several on your side,
            // a small bobbing chevron over it.
            if (player.IsAlive() && !allies.empty())
            {
                const float bob = std::sin(timeSeconds * 5.0f) * 2.0f;
                const float cx = m_Window.WorldToScreenX(player.GetCenterX());
                const float top = m_Window.WorldToScreenY(player.GetY()) - 22.0f + bob;

                for (int row = 0; row < 4; row++)
                {
                    m_Window.DrawScreenRect(
                        cx - 4.0f + static_cast<float>(row), top + static_cast<float>(row),
                        8.0f - static_cast<float>(row) * 2.0f, 1.0f,
                        140, 230, 255, 230);
                }
            }


            // --- Floating combat text (damage numbers, gold, banners) ---
            for (const Floater& floater : floaters)
            {
                // Hold full alpha, then fade over the last 40%.
                const float lifeFrac = floater.Life / floater.MaxLife;
                const Uint8 alpha = static_cast<Uint8>(
                    255.0f * std::min(1.0f, lifeFrac / 0.4f));

                const float x = floater.ScreenSpace
                    ? floater.X
                    : m_Window.WorldToScreenX(floater.X) -
                        PixelFont::Measure(floater.Text, floater.Scale) * 0.5f;
                const float y = floater.ScreenSpace
                    ? floater.Y
                    : m_Window.WorldToScreenY(floater.Y);

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

            // --- Post: vignette, plus a red pulse on damage and at low
            // health ---
            {
                m_Window.DrawVignette(4, 4, 14, 150);

                float redPulse = hurtVignette / 0.6f;

                // Low health: constant heartbeat pulse.
                if (player.IsAlive() && player.GetHealth() * 10 < player.GetMaxHealth() * 3)
                {
                    redPulse = std::max(
                        redPulse,
                        0.3f + 0.25f * std::sin(timeSeconds * 6.0f));
                }

                if (redPulse > 0.0f)
                {
                    m_Window.DrawVignette(190, 20, 24,
                        static_cast<Uint8>(std::min(1.0f, redPulse) * 200.0f));
                }
            }

            // --- HUD ---
            //
            // Vitals bottom left, weapon and items beside them, gold top
            // left, the match state top right; dark translucent panels
            // with a soft gradient and rounded corners, crisp 5x7 labels
            // and chunky numerals for the numbers that matter.

            // Screen-space text with a soft drop shadow so it stays
            // readable over any scene.
            auto drawText = [&](
                float x, float y, float scale,
                const std::string& text,
                Uint8 r, Uint8 g, Uint8 b)
            {
                const float shadow = scale < 2.0f ? 1.0f : scale * 0.5f;

                PixelFont::Draw(m_Window,
                    x + shadow, y + shadow,
                    scale, text, 6, 6, 10, 170);
                PixelFont::Draw(m_Window, x, y, scale, text, r, g, b);
            };

            // A rect with its four corner pixels cut.
            auto roundRect = [&](float x, float y, float w, float h,
                Uint8 r, Uint8 g, Uint8 b, Uint8 a)
            {
                m_Window.DrawScreenRect(x + 1.0f, y, w - 2.0f, h, r, g, b, a);
                m_Window.DrawScreenRect(x, y + 1.0f, 1.0f, h - 2.0f, r, g, b, a);
                m_Window.DrawScreenRect(x + w - 1.0f, y + 1.0f, 1.0f, h - 2.0f, r, g, b, a);
            };

            // HUD panels share the menus' chamfered glass look.
            auto panel = [&](float x, float y, float w, float h)
            {
                ui.Panel(x, y, w, h);
            };

            // A bar with a gradient fill, a lingering damage ghost and
            // optional segment ticks.
            auto drawBar = [&](
                float x, float y, float w, float h,
                float fill,
                Uint8 r, Uint8 g, Uint8 b,
                float ghost = -1.0f,
                int segments = 0)
            {
                fill = std::clamp(fill, 0.0f, 1.0f);

                roundRect(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, 4, 4, 8, 200);
                m_Window.DrawScreenRect(x, y, w, h, 30, 32, 44, 255);

                if (ghost > fill)
                {
                    m_Window.DrawScreenRect(x, y, std::floor(w * ghost), h,
                        static_cast<Uint8>(std::min(255, r / 2 + 90)),
                        static_cast<Uint8>(std::min(255, g / 2 + 70)),
                        static_cast<Uint8>(std::min(255, b / 2 + 60)), 200);
                }

                if (fill > 0.0f)
                {
                    const float fw = std::max(1.0f, std::floor(w * fill));

                    m_Window.DrawGradientScreen(x, y, fw, h,
                        { static_cast<Uint8>(std::min(255, r + 50)),
                          static_cast<Uint8>(std::min(255, g + 50)),
                          static_cast<Uint8>(std::min(255, b + 50)), 255 },
                        { static_cast<Uint8>(r * 3 / 4), static_cast<Uint8>(g * 3 / 4),
                          static_cast<Uint8>(b * 3 / 4), 255 });
                    m_Window.DrawScreenRect(x, y, fw, 1.0f, 255, 255, 255, 70);
                }

                for (int i = 1; i < segments; i++)
                {
                    m_Window.DrawScreenRect(
                        std::floor(x + w * static_cast<float>(i) / segments), y, 1.0f, h,
                        8, 8, 14, 150);
                }
            };

            const float hudBottom = static_cast<float>(ViewHeight) - 16.0f;

            // Vitals: build, health, jet fuel.
            {
                const float px = 16.0f;
                const float pw = 260.0f;
                const float ph = 62.0f;
                const float py = hudBottom - ph;

                panel(px, py, pw, ph);

                const int health = std::max(0, player.GetHealth());
                const float healthFill = health / static_cast<float>(player.GetMaxHealth());
                const bool low = healthFill < 0.3f;

                drawText(px + 12.0f, py + 9.0f, 1.5f,
                    GetBodyClassInfo(player.GetBodyClass()).Name, 150, 158, 180);

                // Health as a big number, its max beside it.
                const std::string maxText = "/" + std::to_string(player.GetMaxHealth());
                const std::string healthText = std::to_string(health);
                const float maxW = PixelFont::Measure(maxText, 1.5f);
                const float numW = PixelFont::Measure(healthText, 3.0f);

                drawText(px + pw - 12.0f - maxW, py + 14.0f, 1.5f, maxText, 120, 126, 146);
                drawText(px + pw - 16.0f - maxW - numW, py + 6.0f, 3.0f, healthText,
                    low ? 255 : 240,
                    low ? 110 : 242,
                    low ? 100 : 248);

                if (const SpriteFrame* icon = SpriteBank::Find("icon_health"))
                    m_Window.DrawSpriteScreen(*icon, px + 12.0f, py + 29.0f);

                drawBar(px + 26.0f, py + 29.0f, pw - 38.0f, 8.0f, healthFill,
                    low ? 230 : 215, low ? 60 : 72, low ? 60 : 64, healthGhost, 10);

                if (const SpriteFrame* icon = SpriteBank::Find("icon_fuel"))
                    m_Window.DrawSpriteScreen(*icon, px + 12.0f, py + 44.0f);

                drawBar(px + 26.0f, py + 46.0f, pw - 38.0f, 4.0f, player.GetFuel(),
                    80, 160, 240);
            }

            // Weapon: name, ammo, reload, and the body's items.
            {
                const float px = 288.0f;
                const float pw = 270.0f;
                const float ph = 62.0f;
                const float py = hudBottom - ph;

                panel(px, py, pw, ph);

                const Weapon& weapon = player.GetWeapon();

                if (weapon.GetDef())
                {
                    std::string name = weapon.GetDef()->Name;

                    for (char& c : name)
                        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

                    drawText(px + 12.0f, py + 9.0f, 1.5f, name, 235, 236, 242);

                    // Ammo: big count, clip size small beside it.
                    if (weapon.IsReloading())
                    {
                        const std::string text = "RELOADING";
                        drawText(px + pw - 12.0f - PixelFont::Measure(text, 1.5f), py + 9.0f, 1.5f,
                            text, 240, 170, 80);
                    }
                    else if (weapon.GetClipSize() > 0)
                    {
                        const std::string clip = "/" + std::to_string(weapon.GetClipSize());
                        const std::string ammo = std::to_string(weapon.GetAmmo());
                        const float clipW = PixelFont::Measure(clip, 1.5f);
                        const bool empty = weapon.GetAmmo() * 4 <= weapon.GetClipSize();

                        drawText(px + pw - 12.0f - clipW, py + 14.0f, 1.5f, clip, 120, 126, 146);
                        drawText(px + pw - 16.0f - clipW - PixelFont::Measure(ammo, 3.0f), py + 6.0f,
                            3.0f, ammo,
                            empty ? 255 : 240, empty ? 180 : 242, empty ? 90 : 248);
                    }
                    else
                    {
                        const std::string text = weapon.IsTool() ? "TOOL" : "INF";
                        drawText(px + pw - 12.0f - PixelFont::Measure(text, 1.5f), py + 9.0f, 1.5f,
                            text, 150, 158, 180);
                    }
                }

                // A thin ammo / reload line under the header.
                if (weapon.IsReloading())
                    drawBar(px + 12.0f, py + 27.0f, pw - 24.0f, 2.0f, weapon.GetReloadProgress(), 240, 160, 70);
                else if (weapon.GetClipSize() > 0)
                    drawBar(px + 12.0f, py + 27.0f, pw - 24.0f, 2.0f,
                        weapon.GetAmmo() / static_cast<float>(weapon.GetClipSize()), 235, 210, 110);

                // Item slots: icon, number key; the selected one lit with
                // an accent underline.
                constexpr float SlotW = 34.0f;
                constexpr float SlotH = 20.0f;
                constexpr float SlotStep = 37.0f;
                const float slotY = py + 35.0f;

                for (int i = 0; i < player.GetItemCount(); i++)
                {
                    const bool selected = i == player.GetSelectedItem();
                    const float slotX = px + 12.0f + static_cast<float>(i) * SlotStep;

                    roundRect(slotX, slotY, SlotW, SlotH,
                        selected ? 70 : 34, selected ? 74 : 36, selected ? 96 : 50, selected ? 255 : 220);

                    if (selected)
                        m_Window.DrawScreenRect(slotX + 2.0f, slotY + SlotH - 2.0f, SlotW - 4.0f, 2.0f,
                            255, 210, 110, 255);

                    const WeaponDef* itemDef = player.GetItem(i).GetDef();
                    const SpriteFrame* icon = itemDef
                        ? SpriteBank::Find("icon_" + WeaponSpriteName(*itemDef))
                        : nullptr;

                    if (icon)
                    {
                        m_Window.DrawSpriteScreen(*icon,
                            std::floor(slotX + (SlotW - icon->Width) * 0.5f + 3.0f),
                            std::floor(slotY + (SlotH - icon->Height) * 0.5f),
                            1.0f, selected ? 255 : 150);
                    }

                    PixelFont::Draw(m_Window, slotX + 3.0f, slotY + 2.0f, 1.0f,
                        std::to_string(i + 1),
                        selected ? 255 : 120, selected ? 220 : 124, selected ? 140 : 140);
                }
            }

            // Gold, top left.
            {
                const std::string goldText = std::to_string(world.GetTeamGold(0));
                const float pw = 34.0f + PixelFont::Measure(goldText, 2.0f);

                panel(16.0f, 16.0f, pw, 26.0f);

                if (const SpriteFrame* gold = SpriteBank::Find("icon_gold"))
                    m_Window.DrawSpriteScreen(*gold, 26.0f, 25.0f);

                drawText(40.0f, 24.0f, 2.0f, goldText, 240, 205, 110);
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
                    240, 242, 248);

                const std::size_t shown = std::min<std::size_t>(enemies.size(), 20);

                for (std::size_t i = 0; i < shown; i++)
                {
                    roundRect(
                        static_cast<float>(ViewWidth) - 28.0f - static_cast<float>(i) * 10.0f,
                        38.0f, 8.0f, 8.0f, 240, 96, 86, 255);
                }
            }

            // Bots-spawn toggle (brightens under the cursor so it reads
            // as clickable).
            {
                const bool on = world.IsBotsEnabled();
                const int hover = botsButtonHovered ? 30 : 0;

                roundRect(BotsButtonX - 1.0f, BotsButtonY - 1.0f, BotsButtonW + 2.0f, BotsButtonH + 2.0f,
                    static_cast<Uint8>(150 + hover), static_cast<Uint8>(160 + hover),
                    static_cast<Uint8>(200 + hover / 2), static_cast<Uint8>(40 + hover * 2));
                m_Window.DrawGradientScreen(BotsButtonX, BotsButtonY, BotsButtonW, BotsButtonH,
                    { 22, 24, 36, 215 }, { 10, 11, 18, 230 });

                // Status light.
                m_Window.DrawScreenRect(BotsButtonX + 10.0f, BotsButtonY + 7.0f, 6.0f, 6.0f,
                    on ? 90 : 220, on ? 220 : 80, on ? 120 : 80, 255);
                m_Window.DrawGlowScreen(BotsButtonX + 13.0f, BotsButtonY + 10.0f, 7.0f,
                    on ? 90 : 220, on ? 220 : 80, on ? 120 : 80, 40);

                drawText(BotsButtonX + 24.0f, BotsButtonY + 7.0f, 1.5f,
                    on ? "BOTS ON" : "BOTS OFF",
                    static_cast<Uint8>(205 + hover), static_cast<Uint8>(210 + hover),
                    static_cast<Uint8>(225 + hover / 2));
            }

            // Multiplayer status under the bots button.
            if (session.GetRole() != SessionRole::Offline)
            {
                std::string status;
                Uint8 r = 170, g = 180, b = 205;

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

                drawText(
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

                panel(panelX, panelY, panelW, 50.0f);

                if (flash)
                    roundRect(panelX - 1.0f, panelY - 1.0f, panelW + 2.0f, 52.0f, 230, 60, 60, 90);

                auto brainBar = [&](float x, const std::string& label, const TeamBase& base,
                    Uint8 r, Uint8 g, Uint8 b)
                {
                    const float w = 180.0f;
                    const float health = base.GetBrainHealth();

                    drawText(x, panelY + 7.0f, 1.5f, label, r, g, b);
                    drawBar(x, panelY + 20.0f, w, 8.0f, health, r, g, b, -1.0f, 10);
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

                    const float sx = m_Window.WorldToScreenX(base.GetBrainCenterX());
                    const float sy = m_Window.WorldToScreenY(base.GetBrainCenterY());

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

            // Minimap: the whole battlefield in the bottom-right corner.
            if (showMinimap)
            {
                const float mapW = 300.0f;
                const float worldW = static_cast<float>(terrain.GetWidth());
                const float worldH = static_cast<float>(terrain.GetHeight());
                const float mapH = std::floor(mapW * worldH / worldW);
                const float mapX = static_cast<float>(ViewWidth) - mapW - 14.0f;
                const float mapY = static_cast<float>(ViewHeight) - mapH - 14.0f;

                minimapTimer -= frameTime;

                if (minimapTimer <= 0.0f || minimap.GetWidth() == 0)
                {
                    minimap.Update(m_Window.GetRenderer(), terrain,
                        static_cast<int>(mapW), static_cast<int>(mapH));
                    minimapTimer = 0.5f;
                }

                m_Window.DrawScreenRect(mapX - 2.0f, mapY - 2.0f, mapW + 4.0f, mapH + 4.0f, 52, 54, 66, 230);
                minimap.Draw(m_Window.GetRenderer(), mapX, mapY);

                auto toMap = [&](float x, float y, float& outX, float& outY)
                {
                    outX = mapX + std::clamp(x / worldW, 0.0f, 1.0f) * mapW;
                    outY = mapY + std::clamp(y / worldH, 0.0f, 1.0f) * mapH;
                };

                auto dot = [&](float x, float y, float size, Uint8 r, Uint8 g, Uint8 b)
                {
                    float mx = 0.0f;
                    float my = 0.0f;
                    toMap(x, y, mx, my);
                    m_Window.DrawScreenRect(mx - size * 0.5f, my - size * 0.5f, size, size, r, g, b, 255);
                };

                // The view.
                {
                    float vx = 0.0f;
                    float vy = 0.0f;
                    toMap(camera.GetX(), camera.GetY(), vx, vy);
                    const float vw = m_Window.GetViewWidth() / worldW * mapW;
                    const float vh = m_Window.GetViewHeight() / worldH * mapH;

                    m_Window.DrawScreenRect(vx, vy, vw, 1.0f, 230, 230, 240, 160);
                    m_Window.DrawScreenRect(vx, vy + vh - 1.0f, vw, 1.0f, 230, 230, 240, 160);
                    m_Window.DrawScreenRect(vx, vy, 1.0f, vh, 230, 230, 240, 160);
                    m_Window.DrawScreenRect(vx + vw - 1.0f, vy, 1.0f, vh, 230, 230, 240, 160);
                }

                for (int team = 0; team < World::TeamCount; team++)
                {
                    const TeamBase& base = world.GetBase(team);

                    if (!base.Exists)
                        continue;

                    const bool blink = !base.BrainAlive || std::fmod(timeSeconds, 1.0f) < 0.75f;

                    if (blink)
                    {
                        dot(base.GetBrainCenterX(), base.GetBrainCenterY(), 6.0f,
                            base.BrainAlive ? (team == 0 ? 120 : 255) : 90,
                            base.BrainAlive ? (team == 0 ? 200 : 118) : 90,
                            base.BrainAlive ? (team == 0 ? 255 : 106) : 90);
                    }
                }

                for (const Enemy& enemy : enemies)
                    dot(enemy.Body->GetCenterX(), enemy.Body->GetCenterY(), 3.0f, 255, 90, 80);

                for (const Ally& ally : allies)
                    dot(ally.Body->GetCenterX(), ally.Body->GetCenterY(), 3.0f, 110, 170, 255);

                for (const Drone& drone : world.GetDrones())
                {
                    dot(drone.X, drone.Y, 2.0f,
                        drone.Team == 0 ? 150 : 255, drone.Team == 0 ? 200 : 140, drone.Team == 0 ? 255 : 120);
                }

                for (int slot = 0; slot < world.GetPlayerCount(); slot++)
                {
                    const Actor& body = *world.GetPlayer(slot).Body;

                    if (body.IsAlive())
                        dot(body.GetCenterX(), body.GetCenterY(), slot == localPlayer ? 5.0f : 4.0f, 255, 255, 255);
                }
            }

            // Death overlay: a dim red wash and the respawn countdown.
            if (!player.IsAlive())
            {
                m_Window.DrawScreenRect(
                    0.0f, 0.0f,
                    static_cast<float>(ViewWidth),
                    static_cast<float>(ViewHeight),
                    60, 6, 10, 90);
                m_Window.DrawVignette(30, 0, 4, 200);

                const std::string label = "REDEPLOYING";
                const float cx = ViewWidth * 0.5f;
                const float cy = ViewHeight * 0.5f;

                drawText(cx - PixelFont::Measure(label, 3.0f) * 0.5f, cy - 26.0f, 3.0f, label,
                    240, 236, 236);
                drawBar(cx - 110.0f, cy + 2.0f, 220.0f, 4.0f,
                    1.0f - localSlot().RespawnTimer / RespawnTime, 235, 235, 245);
            }

            // --- Buy menu, on top of the HUD ---
            {
                BuyMenuContext buyContext = makeBuyContext();
                buyContext.Time = timeSeconds;

                buyMenu.DrawInbound(m_Window, buyContext);

                if (!modal)
                    buyMenu.Frame(m_Window, buyContext);
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
                    ui.Panel(panelX, 110.0f, panelW, 480.0f);

                    if (DrawSettings(ui, panelX + 40.0f, 140.0f))
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

