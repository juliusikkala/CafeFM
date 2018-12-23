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

int main()
{
    init();

    static_fm_synth<OSC_SINE, OSC_SINE, OSC_SINE, OSC_SINE> fm;
    audio_output output(fm, 44100);

    fm.set_frequency(0, 440, output.get_samplerate());
    fm.set_period(1, 0.5);
    fm.set_amplitude(1, 0.2);
    fm.set_period(2, 4);
    fm.set_amplitude(2, 0.1);
    fm.set_period(3, 4);
    fm.set_amplitude(3, 1.0);

    output.start();
    for(int i = 0; i < 100; ++i)
    {
        int x = ((i+9)*(i+6)*(i+2)^3)%12;
        int y = i*(i>>1);
        fm.set_frequency(0, 440*pow(2, x/12.0), output.get_samplerate());
        Pa_Sleep(((y%1)+1)*200);
    }
    output.stop();

    deinit();
    return 0;
}
