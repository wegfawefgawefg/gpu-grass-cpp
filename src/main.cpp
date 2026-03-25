#include "app.h"

#include <exception>

#include <SDL3/SDL.h>

int main(int, char**)
{
    try
    {
        App app;
        app.Run();
        return 0;
    }
    catch (const std::exception& exception)
    {
        SDL_Log("Fatal error: %s", exception.what());
        return 1;
    }
}
