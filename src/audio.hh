#ifndef CAFE_AUDIO_HH
#define CAFE_AUDIO_HH
#include "portaudio.h"
#include <cstdint>
#include <stdexcept>
#include <string>

class audio_output
{
public:
    template<typename Synth>
    audio_output(
        Synth& s,
        unsigned samplerate = 44100,
        unsigned buffer_size = paFramesPerBufferUnspecified
    ): samplerate(samplerate)
    {
        PaError err = Pa_OpenDefaultStream(
            &stream,
            0,
            1,
            paInt32,
            samplerate,
            buffer_size,
            stream_callback<Synth>,
            &s
        );
        if(err != paNoError)
            throw std::runtime_error(
                "Unable to open stream: " + std::string(Pa_GetErrorText(err))
            );
    }

    void start();
    void stop();

    unsigned get_samplerate() const;

    ~audio_output();

private:
    template<typename Synth>
    static int stream_callback(
        const void* input,
        void* output,
        unsigned long framecount,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags flags,
        void* data
    ){
        Synth* synth = static_cast<Synth*>(data);
        synth->synthesize(static_cast<int32_t*>(output), framecount);
        return 0;
    }

    unsigned samplerate;
    PaStream *stream;
};

#endif
