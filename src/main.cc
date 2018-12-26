#include <cstdio>
#include "fm.hh"
#include "audio.hh"
#include <stdexcept>
#include <cmath>
#include <string>
#include <memory>

void init()
{
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
}

using my_synth = fm_synth<OSC_SINE, OSC_SINE, OSC_SINE, OSC_SINE>;

int main()
{
    init();

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
        fm->set_voice_tuning(id2, 0);
        auto id3 = fm->press_voice(x+9);
        auto id4 = fm->press_voice(x+12);
        /*
        for(unsigned i = 0; i < 400; ++i)
        {
            fm->set_voice_tuning(id2, i);
            Pa_Sleep(1);
        }*/
        Pa_Sleep(600);
        fm->release_voice(id);
        fm->release_voice(id2);
        fm->release_voice(id3);
        fm->release_voice(id4);
        Pa_Sleep(200);
    }
    output.stop();

    deinit();
    return 0;
}
