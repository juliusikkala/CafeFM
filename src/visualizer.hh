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
#ifndef CAFEFM_VISUALIZER_HH
#define CAFEFM_VISUALIZER_HH
#include "pffft.h"
#include <vector>
#include <cstdint>
#include <thread>
#include <future>
#include <mutex>

class fm_synth;
struct nk_context;
class visualizer
{
public:
    visualizer(unsigned buffer_size);
    visualizer(const visualizer& other) = delete;
    visualizer(visualizer&& other) = delete;
    ~visualizer();

    void render(nk_context* ctx);
    void start_update(int32_t* samples, unsigned period);
    void start_update(const fm_synth& synth);

private:
    PFFFT_Setup* setup;
    unsigned buffer_size;
    struct data
    {
        unsigned period;
        float freq_max_amp;
        std::vector<float> time;
        std::vector<float> freq; // Stored as interleaved abs(c), arg(c)
    };
    data current;
    std::future<data> incoming;

    // Fills in frequency information & peaks
    void analyze_data(data& d);
};

#endif

