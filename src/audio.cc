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
#include "audio.hh"
#include "helpers.hh"
#include "SDL.h"
#include <sndfile.h>
#include <map>
#include <algorithm>

namespace
{

std::vector<std::pair<const PaHostApiInfo*, PaHostApiIndex>> get_host_apis()
{
    static bool cached = false;
    static std::vector<std::pair<const PaHostApiInfo*, PaHostApiIndex>> res;
    if(!cached)
    {
        int api_count = Pa_GetHostApiCount();
        for(int i = 0; i < api_count; ++i)
        {
            const PaHostApiInfo* info = Pa_GetHostApiInfo(i);
            if(!info || info->deviceCount == 0) continue;
            res.emplace_back(Pa_GetHostApiInfo(i), i);
        }
        cached = true;
    }
    return res;
}

std::vector<std::pair<const PaDeviceInfo*, PaDeviceIndex>> get_devices(
    int system_index
){
    static std::map<
        int,
        std::vector<std::pair<const PaDeviceInfo*, PaDeviceIndex>>
    > cache;
    auto it = cache.find(system_index);
    if(it == cache.end())
    {
        const PaHostApiInfo* api_info;
        PaHostApiIndex index;
        if(system_index >= 0)
        {
            auto pair = get_host_apis()[system_index];
            api_info = pair.first; index = pair.second;
        }
        else
        {
            index = Pa_GetDeviceInfo(Pa_GetDefaultOutputDevice())->hostApi;
            api_info = Pa_GetHostApiInfo(index);
        }

        std::vector<std::pair<const PaDeviceInfo*, PaDeviceIndex>> res;
        int device_count = Pa_GetDeviceCount();
        for(int i = 0; i < device_count; ++i)
        {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if(!info || info->hostApi != index || info->maxOutputChannels == 0)
                continue;
            if(i == api_info->defaultOutputDevice)
                res.insert(res.begin(), std::make_pair(info, i));
            else res.emplace_back(info, i);
        }
        cache[system_index] = res;
        return res;
    }
    return it->second;
}

std::vector<uint64_t> get_samplerates(
    PaDeviceIndex index,
    PaTime target_latency = 0.0,
    int channels = 1,
    PaSampleFormat format = paInt32
){
    static auto cmp = [](
        const PaStreamParameters& a,
        const PaStreamParameters& b
    ){
        return memcmp(&a, &b, sizeof(PaStreamParameters)) < 0;
    };
    static std::map<
        PaStreamParameters,
        std::vector<uint64_t>,
        decltype(cmp)
    > cache(cmp);

    PaStreamParameters output;
    output.device = index;
    output.channelCount = channels;
    output.sampleFormat = format;
    output.suggestedLatency = target_latency;
    output.hostApiSpecificStreamInfo = nullptr;

    auto it = cache.find(output);
    if(it == cache.end())
    {
        constexpr uint64_t try_samplerates[] = {44100, 48000, 96000, 192000};
        std::vector<uint64_t> found_samplerates;

        for(uint64_t samplerate: try_samplerates)
        {

            if(
                Pa_IsFormatSupported(
                    nullptr, &output, samplerate
                ) == paFormatIsSupported
            ) found_samplerates.push_back(samplerate);
        }
        cache[output] = found_samplerates;

        return found_samplerates;
    }
    return it->second;
}

}

audio_output::audio_output(uint64_t samplerate)
:   samplerate(samplerate), ins(nullptr), stream(nullptr), record(false),
    encode(false), encode_head(0), total_recorded_samples(0),
    max_recording_samples(0), loop(samplerate)
{
}

audio_output::~audio_output()
{
    close();
}

void audio_output::open(
    double target_latency,
    int system_index,
    int device_index
){
    close();

    open_stream(
        target_latency,
        system_index,
        device_index,
        stream_callback,
        this
    );
}

void audio_output::close()
{
    if(stream)
    {
        abort_encoding();
        stop();
        Pa_CloseStream(stream);
        ins = nullptr;
        stream = nullptr;
    }
}

void audio_output::start()
{
    if(stream) Pa_StartStream(stream);
}

void audio_output::stop()
{
    if(stream) Pa_StopStream(stream);
}

void audio_output::set_instrument(instrument& i)
{
    if(i.get_samplerate() != samplerate)
        throw std::runtime_error(
            "Instrument samplerate doesn't match audio output!"
        );

    ins = &i;
    loop.set_instrument(ins);
}

void audio_output::start_recording(
    encoder::format fmt,
    double quality,
    double max_recording_length
){
    abort_encoding();

    // 10 second ring buffer should be enough, and uses 7.3 MB @ 192kHz.
    ring_buffer.data.resize(10*samplerate);
    ring_buffer.tail = 0;
    ring_buffer.head = 0;
    raw_recording.clear();
    raw_recording.shrink_to_fit();
    encode_head = 0;
    total_recorded_samples = 0;
    max_recording_samples = max_recording_length*samplerate;

    record = true;
    encode = true;
    enc.reset(new encoder(samplerate, fmt, quality));
    // Recording is done on a different thread so that reallocating the
    // recording vector doesn't ruin the realtime operation of the audio
    // callback.
    recording_thread.reset(
        new std::thread(&audio_output::handle_recording, this)
    );
}

void audio_output::stop_recording()
{
    {
        std::unique_lock<std::mutex> lock(recording_mutex);
        record = false;
    }

    if(recording_thread)
    {
        // Wake up the recording thread and let it notice that record == false.
        recording_cv.notify_one();
    }
}

void audio_output::abort_encoding()
{
    {
        std::unique_lock<std::mutex> lock(recording_mutex);
        record = false;
        encode = false;
    }

    if(recording_thread)
    {
        recording_cv.notify_one();
        recording_thread->join();
        recording_thread.reset();
    }
}

bool audio_output::is_recording() const
{
    return record;
}

bool audio_output::is_encoding() const
{
    return encode;
}

void audio_output::get_encoding_progress(uint64_t& num, uint64_t& denom) const
{
    std::unique_lock<std::mutex> lock(recording_mutex);
    num = total_recorded_samples - raw_recording.size() + encode_head;
    denom = total_recorded_samples;
}

const encoder& audio_output::get_encoder() const
{
    if(!enc)
        throw std::runtime_error("Can't get encoder, nothing has been encoded");
    return *enc;
}

looper& audio_output::get_looper()
{
    return loop;
}

const looper& audio_output::get_looper() const
{
    return loop;
}

uint64_t audio_output::get_samplerate() const
{
    return samplerate;
}

std::vector<const char*> audio_output::get_available_systems()
{
    std::vector<const char*> systems;
    auto apis = get_host_apis();
    for(auto pair: apis) systems.push_back(pair.first->name);
    return systems;
}

std::vector<const char*> audio_output::get_available_devices(
    int system_index
){
    std::vector<const char*> res;
    auto devices = get_devices(system_index);
    for(auto pair: devices) res.push_back(pair.first->name);
    return res;
}

std::vector<uint64_t> audio_output::get_available_samplerates(
    int system_index, int device_index, double target_latency
){
    const PaDeviceInfo* info;
    PaDeviceIndex index;
    if(system_index >= 0)
    {
        auto pair = get_devices(system_index)[device_index];
        info = pair.first;
        index = pair.second;
    }
    else
    {
        index = Pa_GetDefaultOutputDevice();
        info = Pa_GetDeviceInfo(index);
    }

    if(target_latency <= 0) target_latency = info->defaultLowOutputLatency;
    return get_samplerates(index, target_latency);
}

void audio_output::open_stream(
    double target_latency,
    int system_index,
    int device_index,
    PaStreamCallback* callback,
    void* userdata
){
    PaStreamParameters params;

    if(system_index >= 0)
    {
        auto apis = get_host_apis();
        auto devices = get_devices(system_index);
        if(device_index < 0) device_index = 0;
        params.device = devices[device_index].second;
    }
    else params.device = Pa_GetDefaultOutputDevice();

    if(target_latency <= 0)
    {
        target_latency = Pa_GetDeviceInfo(params.device)
            ->defaultLowOutputLatency;
    }

    params.channelCount = 1;
    params.hostApiSpecificStreamInfo = NULL;
    params.sampleFormat = paInt32;
    params.suggestedLatency = target_latency;

    PaError err = Pa_OpenStream(
        &stream,
        nullptr,
        &params,
        samplerate,
        paFramesPerBufferUnspecified,
        paNoFlag,
        callback,
        userdata
    );
    if(err != paNoError)
        throw std::runtime_error(
            "Unable to open stream: " + std::string(Pa_GetErrorText(err))
        );
}

void audio_output::handle_recording()
{
    while(record)
    {
        // First, place new samples from ring buffer to raw recording buffer
        {
            std::unique_lock<std::mutex> lock(recording_mutex);
            uint64_t cur_head = ring_buffer.head;

            size_t old_size = raw_recording.size();

            if(cur_head > ring_buffer.tail)
            {
                size_t size = cur_head - ring_buffer.tail;
                total_recorded_samples += size;
                raw_recording.resize(old_size + size);
                memcpy(
                    raw_recording.data() + old_size,
                    ring_buffer.data.data() + ring_buffer.tail,
                    size * sizeof(int32_t)
                );
            }
            else if(cur_head < ring_buffer.tail)
            {
                size_t end_size = ring_buffer.data.size() - ring_buffer.tail; 
                size_t size = end_size + ring_buffer.head;
                total_recorded_samples += size;
                raw_recording.resize(old_size + size);
                memcpy(
                    raw_recording.data() + old_size,
                    ring_buffer.data.data() + ring_buffer.tail,
                    end_size * sizeof(int32_t)
                );
                memcpy(
                    raw_recording.data() + old_size + end_size,
                    ring_buffer.data.data(),
                    ring_buffer.head * sizeof(int32_t)
                );
            }
            ring_buffer.tail = cur_head;

            if(total_recorded_samples >= max_recording_samples)
            {
                record = false;
                break;
            }
        }

        // Encode if we have nothing better to do.
        handle_encoding();

        // Finally, wait until there's content to handle.
        {
            std::unique_lock<std::mutex> lock(recording_mutex);
            if(record) recording_cv.wait(lock);
        }
    }

    handle_encoding();
}

void audio_output::handle_encoding()
{
    constexpr size_t block_size = 4096;
    constexpr size_t recording_resize_threshold = 1<<20;

    while((ring_buffer.head == ring_buffer.tail || !record) && encode)
    {
        {
            std::unique_lock<std::mutex> lock(recording_mutex);
            size_t size = std::min(
                block_size,
                raw_recording.size() - encode_head
            );
            if(raw_recording.size() <= encode_head) break;
            encode_head += enc->write(raw_recording.data() + encode_head, size);
            if(encode_head >= recording_resize_threshold)
            {
                encode_head -= recording_resize_threshold;
                raw_recording.erase(
                    raw_recording.begin(),
                    raw_recording.begin() + recording_resize_threshold
                );
            }
        }
        std::this_thread::yield();
    }

    {
        std::unique_lock<std::mutex> lock(recording_mutex);
        if(!record && encode)
        {
            enc->finish();
            encode = false;
        }
    }
}

int audio_output::stream_callback(
    const void*,
    void* output,
    unsigned long framecount,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void* data
){
    audio_output* self = static_cast<audio_output*>(data);
    int32_t* o = static_cast<int32_t*>(output);
    size_t sz = framecount * sizeof(*o);
    memset(o, 0, sz);
    self->ins->synthesize(o, framecount);

    // Handle loops
    self->loop.apply(o, framecount);

    // Handle recording final output
    if(self->record)
    {
        int32_t* d = self->ring_buffer.data.data();
        size_t ds = self->ring_buffer.data.size();
        uint64_t head = self->ring_buffer.head;
        
        // Assume ring buffer is large enough that it cannot be wrapped twice.
        if (head + framecount >= ds)
        {
            size_t part1 = ds - head;
            size_t part1_sz = part1 * sizeof(*o);
            memcpy(d + head, o, part1_sz);
            memcpy(d, o + part1, sz - part1_sz);
            head = head + framecount - ds;
        }
        else
        {
            memcpy(d + head, o, sz);
            head += framecount;
        }
        self->ring_buffer.head = head;
        self->recording_cv.notify_one();
    }

    return 0;
}
