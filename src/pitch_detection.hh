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
#ifndef CAFEFM_PITCH_DETECTOR_HH
#define CAFEFM_PITCH_DETECTOR_HH
#include "pffft.h"
#include <vector>

class pitch_detector
{
public:
    pitch_detector(unsigned buffer_size, float samplerate);
    pitch_detector(const pitch_detector& other) = delete;
    pitch_detector(pitch_detector&& other) = delete;
    ~pitch_detector();

    float* get_buffer_data();
    const float* get_buffer_data() const;
    size_t get_buffer_size() const;

    // Returns new period. Update buffer data using get_buffer_data() before
    // calling this.
    float update(float max_period);

private:
    void autocorrelation();
    std::pair<float /*arg*/, float /*value*/>
    quadratic_interpolation(unsigned t, std::vector<float>& data);
    std::vector<unsigned> pick_peaks();

    float prev_period;
    float samplerate;

    PFFFT_Setup* setup;
    std::vector<float> buffer, freq, corr;
};

#endif
