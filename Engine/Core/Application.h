#pragma once

#include "Window.h"

namespace Atlas
{
    class Application
    {
    public:
        Application();

        void Run();

    private:
        Window m_Window;
    };
}