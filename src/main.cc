#include "cafefm.hh"
#include "SDL.h"
#include <stdexcept>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <string>
#include <thread>
#include <memory>
#include <iostream>

void init()
{
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "0");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER))
        throw std::runtime_error(SDL_GetError());

    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if(!(IMG_Init(img_flags) & img_flags))
        throw std::runtime_error(IMG_GetError());

    PaError err = Pa_Initialize();
    if(err != paNoError)
        throw std::runtime_error(
            "Unable to initialize PortAudio: "
            + std::string(Pa_GetErrorText(err))
        );
}

void deinit()
{
    Pa_Terminate();
    IMG_Quit();
    SDL_Quit();
}

int main()
{
    srand(time(nullptr));
    try
    {
        init();
        {
            cafefm app;
            app.load();
            unsigned dt = 0;
            unsigned ms_since_last_render = 0;
            unsigned last_time = SDL_GetTicks();
            while(app.update(dt))
            {
                if(ms_since_last_render > 15)
                {
                    app.render();
                    ms_since_last_render = 0;
                }
                // Play nice with other programs despite checking input really
                // quickly
                else SDL_Delay(1);

                unsigned new_time = SDL_GetTicks();
                dt = new_time - last_time;

                ms_since_last_render += dt;

                last_time = new_time;
            }
        }
        deinit();
    }
    catch(const std::runtime_error& err)
    {
        // Print & save the error message
        std::string what = err.what();

        std::cout << "Runtime error: " << std::endl
                  << what << std::endl;

        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "Runtime error",
            what.c_str(),
            nullptr
        );

        deinit();
        return 1;
    }
    return 0;
}
