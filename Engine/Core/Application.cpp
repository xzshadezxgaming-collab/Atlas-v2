#include "Application.h"

#include "../Actors/Actor.h"
#include "../Graphics/Camera.h"
#include "../Input/Input.h"
#include "../World/Terrain.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>

namespace Atlas
{
    namespace
    {
        constexpr int ViewWidth = 1280;
        constexpr int ViewHeight = 720;

        constexpr int WorldWidth = 2048;
        constexpr int WorldHeight = 1024;

        constexpr float DigRadius = 22.0f;
        constexpr float PlaceRadius = 14.0f;

        // Physics runs at a fixed rate; rendering runs as fast as it can.
        constexpr float FixedTimeStep = 1.0f / 120.0f;
        constexpr float MaxFrameTime = 0.25f;

        bool LoadPlayerSprite(Actor& actor, SDL_Renderer* renderer)
        {
            // The working directory differs between running from the build
            // tree, an IDE, or a packaged install, so try a few locations,
            // ending with the executable's own directory.
            const char* basePath = SDL_GetBasePath();

            const std::string candidates[] =
            {
                "Assets/Player.bmp",
                "../Assets/Player.bmp",
                "../../Assets/Player.bmp",
                "../../../../Assets/Player.bmp",
                basePath
                    ? std::string(basePath) + "Assets/Player.bmp"
                    : std::string(),
            };

            for (const std::string& path : candidates)
            {
                if (path.empty())
                    continue;

                if (actor.LoadBodySprite(renderer, path))
                    return true;
            }

            return false;
        }
    }

    Application::Application()
    {
    }

    void Application::Run()
    {
        std::cout << "=================================\n";
        std::cout << "        ATLAS ENGINE\n";
        std::cout << "           v0.2.0\n";
        std::cout << "=================================\n\n";

        if (!m_Window.Create("Atlas", ViewWidth, ViewHeight))
        {
            return;
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

        Actor player;

        if (!LoadPlayerSprite(player, m_Window.GetRenderer()))
        {
            std::cout << "Failed to load Player.bmp\n";
        }

        player.Spawn(terrain, WorldWidth * 0.5f);

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
                title << "Atlas - " << frames << " FPS";
                m_Window.SetTitle(title.str());

                fpsTimer = 0.0;
                frames = 0;
            }

            running = m_Window.PollEvents();

            Input::Update();

            // Fixed-timestep simulation.
            accumulator += frameTime;

            while (accumulator >= FixedTimeStep)
            {
                float moveInput = 0.0f;

                if (Input::IsKeyDown(SDL_SCANCODE_A))
                    moveInput -= 1.0f;

                if (Input::IsKeyDown(SDL_SCANCODE_D))
                    moveInput += 1.0f;

                player.Update(
                    terrain,
                    FixedTimeStep,
                    moveInput,
                    Input::IsKeyDown(SDL_SCANCODE_SPACE));

                accumulator -= FixedTimeStep;
            }

            // Terrain editing: dig with the left mouse button, place dirt
            // with the right.
            const float mouseWorldX = Input::GetMouseX() + camera.GetX();
            const float mouseWorldY = Input::GetMouseY() + camera.GetY();

            if (Input::IsMouseButtonDown(SDL_BUTTON_LEFT))
            {
                terrain.CarveCircle(mouseWorldX, mouseWorldY, DigRadius);
            }
            else if (Input::IsMouseButtonDown(SDL_BUTTON_RIGHT))
            {
                terrain.PlaceCircle(
                    mouseWorldX,
                    mouseWorldY,
                    PlaceRadius,
                    Material::Dirt);
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

            player.Draw(m_Window);

            // Simple crosshair at the mouse cursor.
            m_Window.DrawFilledRect(
                mouseWorldX - 3.0f,
                mouseWorldY - 1.0f,
                6.0f,
                2.0f,
                255, 255, 255, 200);

            m_Window.DrawFilledRect(
                mouseWorldX - 1.0f,
                mouseWorldY - 3.0f,
                2.0f,
                6.0f,
                255, 255, 255, 200);

            m_Window.EndFrame();
        }
    }
}
