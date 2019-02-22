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
#include "pitch_detection.hh"
#include <cstdio>
#include <complex>
#include <algorithm>
#include <numeric>

pitch_detector::pitch_detector(unsigned buffer_size, float samplerate)
: prev_period(0), samplerate(samplerate)
{
    // Align buffer size.
    buffer_size = determine_compatible_size(buffer_size, 5, 0, 0, 1<<5);
    printf("%u\n", buffer_size);
    setup = pffft_new_setup(buffer_size, PFFFT_REAL);
    buffer.resize(buffer_size, 0);
    corr.resize(buffer_size, 0);
    freq.resize(buffer_size*2, 0);
}

pitch_detector::~pitch_detector()
{
    pffft_destroy_setup(setup);
}

float* pitch_detector::get_buffer_data()
{
    return buffer.data();
}

const float* pitch_detector::get_buffer_data() const
{
    return buffer.data();
}

size_t pitch_detector::get_buffer_size() const
{
    return buffer.size();
}

// Pitch detection technique from here. I tried MPM, YIN and many other real
// PDAs, but none were as good as this one.
// https://github.com/cwilso/PitchDetect/blob/master/js/pitchdetect.js
float pitch_detector::update(float /*max_period*/)
{
    /*
    // First, we do MPM
    autocorrelation();
    std::vector<unsigned> peaks = pick_peaks();
    std::vector<std::pair<float, float>> est;
    float max_amp = -1.0;

    for(unsigned i: peaks)
    {
        std::pair<float, float> e = quadratic_interpolation(i);
        est.push_back(e);
        max_amp = std::max(max_amp, e.second*powf(0.99, e.first));
    }

    if(est.empty()) return prev_period;

    float cutoff = 0.90 * max_amp;
    float mpm_period_est = 0.0;

    for(auto [arg, val]: est)
    {
        if(val >= cutoff)
        {
            mpm_period_est = arg;
            break;
        }
    }

    // Then, the naive "how many times this crosses midpoint" check
    float mid =
        (*std::min_element(buffer.begin(), buffer.end()) +
        *std::max_element(buffer.begin(), buffer.end())) * 0.5f;

    unsigned crossings = 0;
    unsigned i = 1;
    for(; i < buffer.size() && crossings < 4; ++i)
    {
        if(
            (buffer[i-1] < mid && buffer[i] >= mid) ||
            (buffer[i-1] > mid && buffer[i] <= mid)
        ) crossings++;
    }
    float cross_period_est = i*0.5f;
    return cross_period_est;

    // Finally, combine those votes. If they differ too much, pick previous
    // estimate. Otherwise, use MPM estimate as it's probably more accurate.
    float avg_period_est = (cross_period_est + mpm_period_est) * 0.5f;
    if(fabs(cross_period_est-mpm_period_est)/avg_period_est > 0.1)
    {
        return prev_period;
    }
    else
    {
        // Weighted average to keep changes to a minimum.
        prev_period = mpm_period_est*0.1f + prev_period*0.9f;
        return prev_period;
    }

    float max_magnitude = 0.0f;
    float max_magnitude_i = 0;
    std::vector<float> m;
    m.resize(buffer.size());
    pffft_transform_ordered(
        setup,
        buffer.data(),
        freq.data(),
        NULL,
        PFFFT_FORWARD
    );

    for(unsigned i = 0; i < buffer.size(); ++i)
    {
        std::complex<float> c(freq[i*2], freq[i*2+1]);
        m[i] = std::abs(c);
    }

    for(unsigned i = 1; i+1 < m.size(); ++i)
    {
        if(m[i-1] < m[i] && m[i] > m[i+1])
        {
            auto p = quadratic_interpolation(i, m);
            if(p.second > max_magnitude)
            {
                max_magnitude = p.second;
                max_magnitude_i = p.first;
            }
        }
    }

    prev_period = max_magnitude_i*0.1f + prev_period*0.9f;
    return prev_period;
    */

	float max_samples = buffer.size()/2;
	float best_offset = -1;
	float best_correlation = 0;
	bool found_good_correlation = false;
    std::vector<float> correlations(max_samples, 0);

    float rms = 0.0;
    for(unsigned i = 0 ; i < buffer.size(); ++i)
        rms += buffer[i] * buffer[i];
    rms = sqrt(rms/buffer.size());

    if(rms < 0.01) return prev_period;

    float last_correlation = 1;
    for(unsigned offset = 4; offset < max_samples; ++offset)
    {
        float correlation = 0;
        for(unsigned i = 0; i < max_samples; ++i)
            correlation += fabs((buffer[i])-(buffer[i+offset]));
        correlation = 1 - (correlation/max_samples);
        correlations[offset] = correlation;
        if((correlation > 0.9) && (correlation > last_correlation))
        {
            found_good_correlation = true;
            if (correlation > best_correlation)
            {
                best_correlation = correlation;
                best_offset = offset;
            }
        }
        else if(found_good_correlation)
        {
            float shift = (correlations[best_offset+1] - correlations[best_offset-1])/correlations[best_offset];  
            best_offset = (best_offset+(8*shift));
            break;
        }
        last_correlation = correlation;
    }
    float new_period = samplerate/best_offset;
    if(
        best_correlation > 0.01 &&
        fabs(new_period - 2*prev_period)/new_period > 0.2
    ){
        prev_period = (prev_period + new_period)*0.5;
    }
    return prev_period;
}

void pitch_detector::autocorrelation()
{
    pffft_transform_ordered(setup, buffer.data(), freq.data(), NULL, PFFFT_FORWARD);
    float scale = 1.0f/buffer.size();
    for(unsigned i = 0; i < buffer.size(); ++i)
    {
        std::complex<float> c(freq[i*2], freq[i*2+1]);
        c = std::conj(c) * scale;
        freq[i*2] = c.real();
        freq[i*2+1] = c.imag();
    }
    pffft_transform_ordered(setup, freq.data(), corr.data(), NULL, PFFFT_BACKWARD);
}

// https://ccrma.stanford.edu/~jos/sasp/Quadratic_Interpolation_Spectral_Peaks.html
std::pair<float /*arg*/, float /*value*/>
pitch_detector::quadratic_interpolation(unsigned i, std::vector<float>& data)
{
    if(i == 0) i = data[i] < data[i+1] ? i : i+1;
    else if(i >= data.size()-1) i = data[i] < data[i-1] ? i : i-1;
    else
    {
        float num = data[i - 1] - data[i + 1];
        float denom = (data[i - 1] - 2 * data[i] + data[i + 1]);
        if(denom == 0) return {i, data[i]};
        else
        {
            float p = 0.5f * num / denom;
            return {i + p, data[i] - num*p*0.25f};
        }
    }
    return {i, data[i]};
}

std::vector<unsigned> pitch_detector::pick_peaks()
{
    std::vector<unsigned> maxima;
    unsigned i = 0, max_i = 0;

    while(i < (corr.size()-1)/3 && corr[i] > 0) ++i;
    while(i+1 < corr.size() && corr[i] <= 0) ++i;

    i |= !i;

    for(; i < corr.size(); ++i)
    {
        if(i < corr.size() && corr[i] <= 0)
        {
            if(max_i > 0)
            {
                maxima.push_back(max_i);
                max_i = 0;
            }
            while(i+2 < corr.size() && corr[i] <= 0) ++i;
        }

        if(
            corr[i] > corr[i-1] && corr[i] >= corr[i+1] && // Local maximum
            (max_i == 0 || corr[i] > corr[max_i]) // Bigger than previous max
        ) max_i = i;
    }

    if(max_i > 0) maxima.push_back(max_i);

    return maxima;
}
