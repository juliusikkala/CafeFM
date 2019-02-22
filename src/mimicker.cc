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
#include "mimicker.hh"
#include "pffft.h"
#include "helpers.hh"
#include <sndfile.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <limits>
#include <complex>

namespace
{
    std::vector<float> get_blur_kernel(unsigned blur_size)
    {
        std::vector<float> kernel;
        for(unsigned i = 1; i <= blur_size; ++i)
            kernel.push_back(i/(float)blur_size);
        for(unsigned i = blur_size-1; i > 0; --i)
            kernel.push_back(i/(float)blur_size);

        float sum = 0;
        for(float f: kernel) sum += f;
        for(float& f: kernel) f /= sum;

        return kernel;
    }

    std::vector<float> blur(
        const std::vector<float>& freq,
        const std::vector<float>& kernel
    ){
        std::vector<float> out(freq);
        for(int i = 0; i < (int)freq.size(); ++i)
        {
            out[i] = 0;
            int k = i - kernel.size() / 2;
            for(int j = 0; j < (int)kernel.size(); ++j, ++k)
            {
                if(k < 0 || k >= (int)freq.size()) continue;
                out[i] += freq[k] * kernel[j];
            }
        }
        return out;
    }

    double score(
        PFFFT_Setup* setup,
        const fm_synth& synth,
        uint64_t samplerate,
        const std::vector<float>& freq_amp,
        double frequency
    ){
        unsigned buffer_size = freq_amp.size();
        static std::vector<int32_t> samples(buffer_size, 0);
        static std::vector<float> fsamples(buffer_size, 0);
        static std::vector<float> fm_freq(2 * buffer_size, 0);
        static std::vector<float> fm_freq_amp(buffer_size, 0);

        samples.resize(buffer_size);
        fsamples.resize(buffer_size);
        fm_freq.resize(2*buffer_size);
        fm_freq_amp.resize(buffer_size);
        memset(samples.data(), 0, sizeof(int32_t)*buffer_size);
        memset(fm_freq.data(), 0, sizeof(float)*2*buffer_size);

        float total_amplitude = synth.get_total_carrier_amplitude();
        float volume = total_amplitude > 1.0 ? 1.0/total_amplitude : 1.0;
        fm_synth::state s = synth.start(volume);
        synth.set_frequency(s, frequency, samplerate);
        synth.synthesize(s, samples.data(), buffer_size);

        for(unsigned i = 0; i < buffer_size; ++i)
            fsamples[i] = samples[i]/(double)INT32_MAX;

        pffft_transform_ordered(
            setup, fsamples.data(), fm_freq.data(), NULL, PFFFT_FORWARD
        );
        for(unsigned i = 0; i < buffer_size; ++i)
        {
            std::complex<float> c(fm_freq[i*2], fm_freq[i*2+1]);
            fm_freq_amp[i] = std::abs(c);
        }

        // MAE
        double score = 0;
        for(unsigned i = 0; i < buffer_size; ++i)
            score += fabs(fm_freq_amp[i] - freq_amp[i]);
        return score / buffer_size;
    }

    double adjust_last_oscillator(
        PFFFT_Setup* setup,
        fm_synth& synth,
        uint64_t samplerate,
        const std::vector<float>& freq_amp,
        const std::vector<float>& blurred_freq_amp,
        double frequency
    ){
        oscillator& last = synth.get_oscillator(
            synth.get_oscillator_count()-1
        );

        double best_score = score(
            setup, synth, samplerate, blurred_freq_amp, frequency
        );

        // Find coarse period
        unsigned best_num = 1;
        unsigned best_denom = 1;

        for(unsigned i = 1; i <= 8; ++i)
        {
            for(unsigned j = 1; j <= 4; ++j)
            {
                last.set_period(i, j);
                synth.update_period_lookup();
                float cur_score = score(
                    setup, synth, samplerate, blurred_freq_amp, frequency
                );
                if(cur_score < best_score)
                {
                    best_score = cur_score;
                    best_num = i;
                    best_denom = j;
                }
            }
        }
        last.set_period(best_num, best_denom);
        synth.update_period_lookup();

        // Find fine period
        best_score = score(setup, synth, samplerate, freq_amp, frequency);
        constexpr double period_step_count = 32;
        double fine_range = 0.5/best_denom;
        double fine_step = fine_range / (period_step_count-1);
        double best_fine = 0;
        for(unsigned i = 0; i < period_step_count; ++i)
        {
            double fine = i * fine_step - fine_range;
            last.set_period_fine(fine);
            synth.update_period_lookup();
            float cur_score = score(
                setup, synth, samplerate, freq_amp, frequency
            );
            if(cur_score < best_score)
            {
                best_score = cur_score;
                best_fine = fine;
            }
        }
        last.set_period_fine(best_fine);
        synth.update_period_lookup();

        // Find amplitude
        constexpr double amp_step_count = 32;
        double best_amp = 0.5;
        for(unsigned i = 0; i < amp_step_count; ++i)
        {
            double amp = i / (amp_step_count-1.0);
            last.set_amplitude(amp);
            synth.update_period_lookup();
            float cur_score = score(
                setup, synth, samplerate, freq_amp, frequency
            );
            if(cur_score < best_score)
            {
                best_score = cur_score;
                best_amp = amp;
            }
        }
        last.set_amplitude(best_amp);
        synth.update_period_lookup();
        if(best_amp < 0.0001) return -1.0;
        return best_score;
    }
}

fm_synth mimic_sample(const fs::path& path)
{
    // Read audio file
    std::string path_str = path.string();
    SF_INFO info;
    memset(&info, 0, sizeof(info));
    SNDFILE* f = sf_open(path_str.c_str(), SFM_READ, &info);
    if(!f) throw std::runtime_error("Unable to read file " + path_str);

    constexpr unsigned max_length = 1<<16;
    if(info.frames > max_length)
        throw std::runtime_error(
            "This audio file is too long. The maximum length at this "
            "samplerate is " + std::to_string(max_length/(float)info.samplerate)
            + " seconds. Please lower the samplerate or cut the sample shorter."
        );

    // Read audio data
    std::vector<float> data, signal, freq, freq_amp;
    data.resize(info.channels * info.frames, 0);
    sf_readf_float(f, data.data(), info.frames);

    // Take first channel as our signal
    signal.resize(info.frames, 0);
    for(unsigned i = 0; i < info.frames; ++i)
        signal[i] = data[i*info.channels];

    // Create frequency domain representation of sample signal
    size_t buffer_size =
        determine_pffft_compatible_size_max(info.frames, 5, 0, 0, 1<<5);
    PFFFT_Setup* setup = pffft_new_setup(buffer_size, PFFFT_REAL);

    freq.resize(buffer_size * 2, 0);
    freq_amp.resize(buffer_size, 0);
    pffft_transform_ordered(
        setup, signal.data(), freq.data(), NULL, PFFFT_FORWARD
    );
    for(unsigned i = 0; i < buffer_size; ++i)
    {
        std::complex<float> c(freq[i*2], freq[i*2+1]);
        freq_amp[i] = std::abs(c);
    }

    // Create blurred frequency representation for easier frequency matching for
    // modulators.
    std::vector<float> kernel = get_blur_kernel(buffer_size/128);
    std::vector<float> blurred_freq_amp = blur(freq_amp, kernel);

    fm_synth best_fm, cur_fm;
    best_fm.set_modulation_mode(fm_synth::PHASE);

    // Find match for fundamental frequency, use it only for scoring.
    auto fundamental_it = std::max_element(freq_amp.begin(), freq_amp.end());
    unsigned fundamental_i = fundamental_it - freq_amp.begin();
    double fundamental_freq = 
        fundamental_i / (double)buffer_size * info.samplerate;
    float amp = 0;
    for(unsigned i = 0; i < buffer_size; ++i)
        if(amp < fabs(signal[i])) amp = fabs(signal[i]);

    best_fm.get_oscillator(0).set_amplitude(amp);
    best_fm.finish_changes();
    double best_score = score(
        setup, best_fm, info.samplerate, freq_amp, fundamental_freq
    );
    cur_fm = best_fm;

    for(unsigned i = 0; i < 4; ++i)
    {
        unsigned new_id = cur_fm.add_oscillator({});
        cur_fm.get_oscillator(i).get_modulators().push_back(new_id);
        cur_fm.finish_changes();

        double cur_score = adjust_last_oscillator(
            setup, cur_fm, info.samplerate, freq_amp, blurred_freq_amp,
            fundamental_freq
        );
        if(cur_score < 0) break;

        if(cur_score < best_score)
        {
            best_score = cur_score;
            best_fm = cur_fm;
        }
    }

    best_fm.limit_total_carrier_amplitude();
    return best_fm;
}
