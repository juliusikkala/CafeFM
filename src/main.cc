#include <cstdio>
#include "fm.hh"
#include "audio.hh"
#include <stdexcept>
#include <cmath>
#include <string>

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

using my_synth = static_fm_synth<OSC_SINE, OSC_SINE, OSC_SINE, OSC_SINE>;

int main()
{
    init();

    my_synth fm(44100);
    fm.set_polyphony(3);
    my_synth::stack stack;
    stack.set_period(1, 0.5);
    stack.set_amplitude(1, 0.2);
    stack.set_period(2, 4);
    stack.set_amplitude(2, 0.5);
    stack.set_period(3, 1);
    stack.set_amplitude(3, 1.0);
    fm.set_volume(0.3f);
    fm.set_stack(stack);

    audio_output output(fm);

    output.start();
    for(int i = 0; i < 100; ++i)
    {
        int x = ((i+9)*(i+6)*(i+2)^3)%12;
        int y = i*(i>>1);
        auto id = fm.press_voice(x);
        auto id2 = fm.press_voice(x-3);
        auto id3 = fm.press_voice(x-7);
        Pa_Sleep(600);
        fm.release_voice(id);
        fm.release_voice(id2);
        fm.release_voice(id3);
        Pa_Sleep(200);
    }
    output.stop();

    deinit();
    return 0;
}
