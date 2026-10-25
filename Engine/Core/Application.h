#pragma once

#include "Settings.h"
#include "Window.h"

#include <cstdint>
#include <string>

namespace Atlas
{
    class Ui;

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

        // Endless waves instead of brain-versus-brain (--survival).
        bool Survival = false;

        // Set when the command line asked for a match directly, so the
        // main menu is skipped the first time.
        bool SkipMenu = false;

        // Parses --host [port] and --join <address>[:port]. Returns false
        // (and prints usage) on bad arguments.
        bool Parse(int argc, char** argv);
    };

    // Everything needed to start a match, as chosen in the menus (or on
    // the command line).
    struct MatchRequest
    {
        LaunchMode Mode = LaunchMode::Offline;
        std::string Address = "127.0.0.1";
        std::uint16_t Port = 27960;

        bool Survival = false;
        int MapSize = 1;     // 0 small, 1 medium, 2 large
        int Difficulty = 1;  // 0 easy, 1 normal, 2 hard
        int LandingZone = 12; // % of map width from the west
        unsigned int Seed = 0; // 0 = random
        unsigned int RandomSeed = 0; // the roll for a random map, 0 = roll at start
    };

    enum class MatchExit
    {
        Quit,    // close the game
        ToMenu,  // back to the main menu
        Restart, // same settings, new match
    };

    class Application
    {
    public:
        explicit Application(const LaunchOptions& options = LaunchOptions());

        void Run();

    private:
        // Main menu and its screens. Returns false to quit.
        bool RunMenu(MatchRequest& request);

        MatchExit RunMatch(const MatchRequest& request);

        // The settings screen, drawn inside a menu or the pause overlay.
        // Returns true when Back was pressed.
        bool DrawSettings(Ui& ui, float x, float y);

        void ApplySettings();

        LaunchOptions m_Options;
        Settings m_Settings;
        Window m_Window;
    };
}
