#include "audio.hh"

audio_output::~audio_output()
{
    Pa_CloseStream(stream);
}

void audio_output::start()
{
    Pa_StartStream(stream);
}

void audio_output::stop()
{
    Pa_StopStream(stream);
}

unsigned audio_output::get_samplerate() const
{
    return samplerate;
}
