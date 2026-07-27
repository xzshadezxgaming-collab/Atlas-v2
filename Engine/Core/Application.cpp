#include "Application.h"

#include "../Graphics/Camera.h"
#include "../Graphics/Sprite.h"
#include "../Input/Input.h"
#include "../Physics/RigidBody.h"
#include "../World/Terrain.h"

#include <SDL3/SDL.h>

#include <iostream>
#include <sstream>

namespace Atlas
{
    namespace
    {
        constexpr float PlayerWidth = 64.0f;
        constexpr float PlayerHeight = 64.0f;
        constexpr float MoveSpeed = 300.0f;
        constexpr float JumpVelocity = -550.0f;
    }

    Application::Application()
    {
    }

    void Application::Run()
    {
        std::cout << "=================================\n";
        std::cout << "        ATLAS ENGINE\n";
        std::cout << "           v0.1.8\n";
        std::cout << "=================================\n\n";

        if (!m_Window.Create("Atlas", 1280, 720))
        {
            return;
        }

        Camera camera;
        m_Window.SetCamera(&camera);

        Terrain terrain;

        Sprite player;

        if (!player.Load(
            m_Window.GetRenderer(),
            "../../../../Assets/Player.bmp"))
        {
            std::cout << "Failed to load Player.bmp\n";
        }

        player.SetSize(PlayerWidth, PlayerHeight);

        RigidBody body;
        body.SetPosition(0.0f, 0.0f);

        bool running = true;

        Uint64 lastCounter = SDL_GetPerformanceCounter();
        Uint64 frequency = SDL_GetPerformanceFrequency();

        double timer = 0.0;
        int frames = 0;

        while (running)
        {
            Uint64 currentCounter = SDL_GetPerformanceCounter();

            float deltaTime = static_cast<float>(
                static_cast<double>(currentCounter - lastCounter) /
                static_cast<double>(frequency));

            lastCounter = currentCounter;

            timer += deltaTime;
            frames++;

            if (timer >= 1.0)
            {
                std::stringstream title;
                title << "Atlas - " << frames << " FPS";
                m_Window.SetTitle(title.str());

                timer = 0.0;
                frames = 0;
            }

            running = m_Window.PollEvents();

            Input::Update();

            float velocityX = 0.0f;

            if (Input::IsKeyDown(SDL_SCANCODE_A))
                velocityX -= MoveSpeed;

            if (Input::IsKeyDown(SDL_SCANCODE_D))
                velocityX += MoveSpeed;

            float velocityY = body.GetVelocityY();

            if (Input::IsKeyDown(SDL_SCANCODE_SPACE) && body.IsGrounded())
            {
                velocityY = JumpVelocity;
                body.SetGrounded(false);
            }

            body.SetVelocity(
                velocityX,
                velocityY);

            body.Update(deltaTime);

            if (terrain.IsSolid(
                body.GetX() + PlayerWidth * 0.5f,
                body.GetY() + PlayerHeight))
            {
                while (terrain.IsSolid(
                    body.GetX() + PlayerWidth * 0.5f,
                    body.GetY() + PlayerHeight))
                {
                    body.SetPosition(
                        body.GetX(),
                        body.GetY() - 1.0f);
                }

                body.SetVelocity(
                    body.GetVelocityX(),
                    0.0f);

                body.SetGrounded(true);
            }

            player.SetPosition(
                body.GetX(),
                body.GetY());

            camera.SetPosition(
                body.GetX() - 640.0f + PlayerWidth * 0.5f,
                body.GetY() - 360.0f + PlayerHeight * 0.5f);

            m_Window.BeginFrame();

            terrain.Draw(m_Window);

            player.Draw(m_Window);

            m_Window.EndFrame();
        }
    }
}