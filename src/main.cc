#include "cafefm.hh"
#include "SDL.h"
#include <stdexcept>
#include <cmath>
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
    try
    {
        init();
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
            else std::this_thread::yield();

            unsigned new_time = SDL_GetTicks();
            dt = new_time - last_time;

            ms_since_last_render += dt;

            last_time = new_time;
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

    /*
    std::unique_ptr<basic_fm_synth> fm(create_fm_synth(
        44100,
        OSC_SINE,
        {
            {OSC_SINE, 0.5, 0.3},
            {OSC_SINE, 2, 0.5},
            {OSC_SINE, 0.5, 0.4}
        }
    ));
    fm->set_polyphony(16);
    //fm->set_volume(1.0/4);
    fm->set_max_safe_volume();

    audio_output output(*fm);
    output.start();

    for(int i = 0; i < 100; ++i)
    {
        int x = ((i*i*i*i*i+256)%12)-20;
        auto id = fm->press_voice(x);
        auto id2 = fm->press_voice(x+3);
        auto id3 = fm->press_voice(x+9);
        auto id4 = fm->press_voice(x+12);
        Pa_Sleep(600);
        fm->release_voice(id);
        fm->release_voice(id2);
        fm->release_voice(id3);
        fm->release_voice(id4);
        Pa_Sleep(200);
    }

    output.stop();
    */
    return 0;
}
