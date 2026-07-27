#include "Application.h"

#include "../ECS/Entity.h"
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

        constexpr float PlayerWidth = 48.0f;
        constexpr float PlayerHeight = 48.0f;
        constexpr float MoveSpeed = 220.0f;
        constexpr float JumpVelocity = -480.0f;

        constexpr float DigRadius = 22.0f;
        constexpr float PlaceRadius = 14.0f;

        // Physics runs at a fixed rate; rendering runs as fast as it can.
        constexpr float FixedTimeStep = 1.0f / 120.0f;
        constexpr float MaxFrameTime = 0.25f;

        bool LoadPlayerSprite(Sprite& sprite, SDL_Renderer* renderer)
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

                if (sprite.Load(renderer, path))
                    return true;
            }

            return false;
        }

        float FindSpawnY(const Terrain& terrain, float x)
        {
            for (int y = 0; y < terrain.GetHeight(); y++)
            {
                if (terrain.IsSolid(x, static_cast<float>(y)))
                {
                    return static_cast<float>(y) - PlayerHeight - 2.0f;
                }
            }

            return 0.0f;
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

        Entity player;

        if (!LoadPlayerSprite(player.GetSprite(), m_Window.GetRenderer()))
        {
            std::cout << "Failed to load Player.bmp\n";
        }

        player.GetSprite().SetSize(PlayerWidth, PlayerHeight);
        player.GetRigidBody().SetSize(PlayerWidth, PlayerHeight);

        const float spawnX = WorldWidth * 0.5f;

        player.GetTransform().SetPosition(
            spawnX - PlayerWidth * 0.5f,
            FindSpawnY(terrain, spawnX));

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
                RigidBody& body = player.GetRigidBody();

                float velocityX = 0.0f;

                if (Input::IsKeyDown(SDL_SCANCODE_A))
                    velocityX -= MoveSpeed;

                if (Input::IsKeyDown(SDL_SCANCODE_D))
                    velocityX += MoveSpeed;

                float velocityY = body.GetVelocityY();

                if (Input::IsKeyDown(SDL_SCANCODE_SPACE) && body.IsGrounded())
                {
                    velocityY = JumpVelocity;
                }

                body.SetVelocity(velocityX, velocityY);
                body.Update(terrain, FixedTimeStep);

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
            const float playerCenterX =
                player.GetTransform().GetX() + PlayerWidth * 0.5f;
            const float playerCenterY =
                player.GetTransform().GetY() + PlayerHeight * 0.5f;

            camera.SetPosition(
                std::clamp(
                    playerCenterX - ViewWidth * 0.5f,
                    0.0f,
                    static_cast<float>(WorldWidth - ViewWidth)),
                std::clamp(
                    playerCenterY - ViewHeight * 0.5f,
                    0.0f,
                    static_cast<float>(WorldHeight - ViewHeight)));

            m_Window.BeginFrame();

            terrain.Draw(m_Window);

            player.GetSprite().Draw(m_Window);

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
