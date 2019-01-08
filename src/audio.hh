/*
    Copyright 2018-2019 Julius Ikkala

    This file is part of CafeFM.

    CafeFM is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    CafeFM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with CafeFM.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CAFE_AUDIO_HH
#define CAFE_AUDIO_HH
#include "portaudio.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

class audio_output
{
public:
    template<typename Synth>
    audio_output(
        Synth& s,
        double target_latency = 0.030,
        int system_index = -1,
        int device_index = -1
    ): samplerate(s.get_samplerate())
    {
        open_stream(
            target_latency,
            system_index,
            device_index,
            stream_callback<Synth>,
            &s
        );
    }

    ~audio_output();

    void start();
    void stop();

    unsigned get_samplerate() const;

    static std::vector<const char*> get_available_systems();
    static std::vector<const char*> get_available_devices(
        int system_index
    );
    static std::vector<uint64_t> get_available_samplerates(
        int system_index, int device_index, double target_latency = 0.030
    );

private:
    void open_stream(
        double target_latency,
        int system_index,
        int device_index,
        PaStreamCallback* callback,
        void* userdata
    );

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
        int32_t* o = static_cast<int32_t*>(output);
        memset(o, 0, framecount * sizeof(*o));
        synth->synthesize(o, framecount);

        return 0;
    }

    unsigned samplerate;
    PaStream *stream;
};

#endif
