/*
    Copyright 2019 Julius Ikkala

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
#ifndef CAFEFM_CONTROLLER_MICROPHONE_HH
#define CAFEFM_CONTROLLER_MICROPHONE_HH
#include "controller.hh"
#include "portaudio.h"
#include <atomic>
#include <vector>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <memory>
#include <chrono>

// Quite simplistic at the moment, only finds the default microphone.
class microphone: public controller
{
private:
    microphone(PaDeviceIndex index);
public:
    microphone(const microphone& other) = delete;
    microphone(microphone&& other) = delete;
    ~microphone();

    static std::vector<microphone*> discover();

    bool poll(change_callback cb = {}, bool active = true) override;

    bool assign_bind_on_use() const override;
    bool potentially_inactive() const override;

    std::string get_type_name() const override;
    std::string get_device_name() const override;

    unsigned get_axis_count() const override;
    std::string get_axis_name(unsigned i) const override;
    axis get_axis_state(unsigned i) const override;

private:
    static int stream_callback(
        const void* input,
        void* output,
        unsigned long framecount,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags flags,
        void* data
    );

    void analyzer();

    PaStream* stream;
    std::atomic<float> pitch;
    std::atomic<float> volume;
    bool was_active;

    std::vector<float> buffer;
    std::atomic_uint head, buffer_samples;

    bool analyzer_should_quit;
    unsigned analyzer_samples;
    double samplerate;
    std::condition_variable analyzer_cv;
    std::unique_ptr<std::thread> analyzer_thread;
};

#endif
