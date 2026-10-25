#include "Core/Application.h"

int main(int argc, char** argv)
{
    Atlas::LaunchOptions options;

    if (!options.Parse(argc, argv))
        return 1;

    Atlas::Application app(options);

    app.Run();

    return 0;
}
