#pragma once

#include "Window.h"

#include <cstdint>
#include <string>

namespace Atlas
{
    enum class LaunchMode
    {
        Offline,
        Host,
        Join,
    };

    struct LaunchOptions
    {
        LaunchMode Mode = LaunchMode::Offline;
        std::string Address = "127.0.0.1";
        std::uint16_t Port = 27960;

        // Parses --host [port] and --join <address>[:port]. Returns false
        // (and prints usage) on bad arguments.
        bool Parse(int argc, char** argv);
    };

    class Application
    {
    public:
        explicit Application(const LaunchOptions& options = LaunchOptions());

        void Run();

    private:
        LaunchOptions m_Options;
        Window m_Window;
    };
}
