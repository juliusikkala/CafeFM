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
#include "microphone.hh"
#include "../func.hh"
#include <numeric>
#define MIN_FREQUENCY 80

namespace
{
    // https://ccrma.stanford.edu/~jos/sasp/Quadratic_Interpolation_Spectral_Peaks.html
    float quadratic_interpolation(unsigned i, const std::vector<float>& data)
    {
        if(i == 0) i = data[i] < data[i+1] ? i : i+1;
        else if(i >= data.size()-1) i = data[i] < data[i-1] ? i : i-1;
        else
        {
            float num = data[i - 1] - data[i + 1];
            float denom = (data[i - 1] - 2 * data[i] + data[i + 1]);
            if(denom == 0) return i;
            else return i + 0.5f * num / denom;
        }
        return i;
    }

    float calculate_rms(const std::vector<float>& buffer)
    {
        return sqrtf(
            std::inner_product(
                buffer.begin(), buffer.end(),
                buffer.begin(), 0.0f
            )/buffer.size()
        );
    }

    // Based on the ACF2+ algorithm:
    // http://hellanicus.lib.aegean.gr/bitstream/handle/11610/8650/file1.pdf
    // I'm not Greek, so I read the implementation from here:
    // https://github.com/cwilso/PitchDetect/blob/b0d5d28d2803d852dd85d2a1e53c22bcedba4cbf/js/pitchdetect.js#L283
    // It's from a pull request by "dalatant", who is apparently the author
    // of that thesis.
    //
    // The algorithm's specialty seems to be picking suitable start and end
    // points and picking the peak in a way that skips bad early peaks pretty
    // well.
    //
    // Some modifications to the core algorithm have been made to smooth the
    // output and remove unneeded features.
    float detect_period(const std::vector<float>& buffer, float& prev_period)
    {
        // Avoid messy output with low signal
        if(calculate_rms(buffer) < 0.1) return prev_period;

        unsigned half = buffer.size()/2;
        auto threshold = [](float v){ return fabs(v) < 0.2; };

        // Find good start and end points.
        unsigned start = std::find_if(buffer.begin(), buffer.begin() + half,
            threshold) - buffer.begin();

        unsigned end = buffer.size() - (std::find_if(
            buffer.rbegin(), buffer.rbegin() + half, threshold
        ) - buffer.rbegin()) - 1;

        if(end <= start) return prev_period;

        // Autocorrelation for the selected segment.
        // TODO: Do this in frequency space for that sweet, sweet n*log(n)
        unsigned len = end - start;
        std::vector<float> corr(len, 0.0);
        for(unsigned i = 0; i < len; ++i)
        {
            corr[i] = std::inner_product(
                buffer.begin() + start,
                buffer.begin() + end - i,
                buffer.begin() + start + i,
                0.0f
            );
        }

        // Peak picking algorithm.
        unsigned i = 0;
        // Skip lowering correlations.
        while(corr[i] > corr[i+1] && i + 2 < corr.size()) ++i;

        // Find highest peak from that point on.
        unsigned max_i = std::max_element(
            corr.begin() + i, corr.end()
        ) - corr.begin();

        // Peak interpolation to get better accuracy.
        float new_period = quadratic_interpolation(max_i, corr);

        // Smooth output & octave jump protection
        prev_period = (prev_period + new_period)*0.5;
        return prev_period;
    }

    // Using PEAK algorithm
    float detect_level(const std::vector<float>& buffer, float samplerate)
    {
        constexpr float ta = 0.010;
        constexpr float tr = 0.200;
        float Ts = 1.0f / samplerate;
        float AT = 1.0 - exp(-2.2 * Ts / ta);
        float RT = 1.0 - exp(-2.2 * Ts / tr);

        float x_peak = fabs(buffer[0]);
        for(float x: buffer)
        {
            if(fabs(x) > x_peak) x_peak = (1.0 - AT) * x_peak + AT * fabs(x);
            else x_peak = (1.0 - RT) * x_peak;
        }
        return x_peak;
    }
}

microphone::microphone(PaDeviceIndex index)
: stream(nullptr), was_active(false)
{
    const PaDeviceInfo* info = Pa_GetDeviceInfo(index);
    PaStreamParameters input;
    input.device = index;
    input.channelCount = 1;
    input.sampleFormat = paFloat32;
    input.suggestedLatency = info->defaultLowInputLatency;
    input.hostApiSpecificStreamInfo = nullptr;
    samplerate = info->defaultSampleRate;

    PaError err = Pa_OpenStream(
        &stream,
        &input,
        NULL,
        samplerate,
        paFramesPerBufferUnspecified,
        paNoFlag,
        stream_callback,
        this
    );
    if(err != paNoError) stream = nullptr;

    // Two seconds should be more than enough.
    buffer.resize(samplerate*2, 0.0f);
    head = buffer_samples = 0;
    analyzer_should_quit = false;
    analyzer_samples = ceil(samplerate/MIN_FREQUENCY);
    pitch = 0.0f;
    volume = 0.0f;
}

microphone::~microphone()
{
    if(stream)
    {
        analyzer_should_quit = true;
        Pa_CloseStream(stream);
        analyzer_cv.notify_all();

        if(analyzer_thread)
        {
            analyzer_thread->join();
            analyzer_thread.reset();
        }
    }
}

std::vector<microphone*> microphone::discover()
{
    static bool first = true;
    // This just creates the default microphone for now.
    if(first)
    {
        first = false;
        PaDeviceIndex index = Pa_GetDefaultInputDevice();
        if(index == paNoDevice) return {};
        return {new microphone(index)};
    }
    else return {};
}

bool microphone::poll(change_callback cb, bool active)
{
    // Check activeness for disabling analyzer when disconnected
    if(!active && was_active)
    {
        analyzer_should_quit = true;
        Pa_AbortStream(stream);
        analyzer_cv.notify_all();

        if(analyzer_thread)
        {
            analyzer_thread->join();
            analyzer_thread.reset();
        }
    }
    else if(active && !was_active)
    {
        analyzer_should_quit = false;
        buffer_samples = 0;
        Pa_StartStream(stream);
        analyzer_thread.reset(new std::thread(&microphone::analyzer, this));
    }
    was_active = active;

    cb(this, 0, -1);
    cb(this, 1, -1);

    return !!stream;
}

bool microphone::assign_bind_on_use() const { return false; }
bool microphone::potentially_inactive() const { return true; }

std::string microphone::get_type_name() const
{
    return "Microphone";
}

std::string microphone::get_device_name() const
{
    return "Default microphone";
}

unsigned microphone::get_axis_count() const
{
    return 2;
}

std::string microphone::get_axis_name(unsigned i) const
{
    constexpr const char* const names[2] = {
        "Pitch", "Volume"
    };
    return names[i];
}

axis microphone::get_axis_state(unsigned i) const
{
    axis res;
    switch(i)
    {
    default:
    case 0:
        res.is_limited = false;
        res.is_signed = true;
        res.value = pitch;
        break;
    case 1:
        res.is_limited = true;
        res.is_signed = false;
        res.value = volume;
        break;
    }
    return res;
}

int microphone::stream_callback(
    const void* input,
    void*,
    unsigned long framecount,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void* data
){
    microphone* self = static_cast<microphone*>(data);
    const float* fin = static_cast<const float*>(input);
    unsigned new_head = self->head;
    for(unsigned i = 0; i < framecount; ++i)
    {
        self->buffer[new_head] = fin[i];
        new_head++;
        if(new_head == self->buffer.size()) new_head = 0;
    }

    self->head = new_head;
    self->buffer_samples += framecount;
    self->analyzer_cv.notify_one();
    return paContinue;
}

void microphone::analyzer()
{
    std::mutex analyzer_mutex;
    unsigned h = 0;
    std::vector<float> tmp(analyzer_samples, 0.0f);
    float prev_period = 0;

    while(!analyzer_should_quit)
    {
        if(head == h)
        {
            std::unique_lock<std::mutex> lk(analyzer_mutex);
            analyzer_cv.wait(lk);
            if(analyzer_should_quit) break;
        }
        unsigned samples = buffer_samples;
        if(samples < analyzer_samples || head == h) continue;
        h = head;

        // Unwrap buffer for easier handling. This should be so fast that, even
        // though technically unnecessary, it doesn't matter at all.
        int read_h = h;
        for(unsigned i = 0; i < analyzer_samples; ++i)
        {
            read_h--;
            if(read_h < 0) read_h = buffer.size()-1;
            tmp[analyzer_samples-1-i] = buffer[read_h];
        }

        float period = detect_period(tmp, prev_period);
        float frequency = samplerate/period;
        pitch = 12.0*log2f(frequency/440.0f);
        volume = detect_level(tmp, samplerate);
    }
}
