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
#include "visualizer.hh"
#include "fm.hh"
#include "nuklear.hh"
#include <future>
#include <complex>

namespace
{
    unsigned determine_compatible_size(
        unsigned buffer_size, unsigned a, unsigned b, unsigned c, unsigned d
    ){
        if(d >= buffer_size) return d;

        unsigned out = 0;
        if(b == 0)
        {
            out = determine_compatible_size(buffer_size, a+1, b, c, d*2);
            out = std::min(
                determine_compatible_size(buffer_size, a, b+1, c, d*3), out
            );
            out = std::min(
                determine_compatible_size(buffer_size, a, b, c+1, d*5), out
            );
        }
        else if(c == 0)
        {
            out = determine_compatible_size(buffer_size, a, b+1, c, d*3);
            out = std::min(
                determine_compatible_size(buffer_size, a, b, c+1, d*5), out
            );
        }
        else
        {
            out = determine_compatible_size(buffer_size, a, b, c+1, d*5);
        }
        return out;
    }
}

visualizer::visualizer(unsigned buffer_size)
{
    // Align buffer size.
    this->buffer_size = determine_compatible_size(buffer_size, 5, 0, 0, 1<<5);
    setup = pffft_new_setup(this->buffer_size, PFFFT_REAL);
    current.period = 0;
    current.time.resize(buffer_size, 0);
    current.freq.resize(buffer_size*2, 0);
}

visualizer::~visualizer()
{
    pffft_destroy_setup(setup);
}

void visualizer::render(nk_context* ctx)
{
    // Check for new data.
    if(
        incoming.valid() &&
        incoming.wait_for(std::chrono::seconds(0)) == std::future_status::ready
    ) current = incoming.get();

    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    struct nk_rect s = nk_window_get_content_region(ctx);
    struct nk_rect t = s;
    static constexpr int pad = 4;
    t.x += pad;
    t.y += pad;
    t.w = s.w/2 - 2*pad;
    t.h = s.h - 2*pad;
    double spacing = s.w/2;

    nk_layout_space_begin(ctx, NK_STATIC, s.h, 1);

    const struct nk_color bg_color = nk_rgb(38, 32, 30);
    const struct nk_color line_color = nk_rgb(175, 150, 130);
    // Plot time domain data
    nk_fill_rect(canvas, t, 4.0, bg_color);
    float prev_x = t.x; 
    float prev_y = t.y + t.h*0.5 + current.time[0]*t.h*0.5; 
    for(unsigned i = 1; i < current.period; ++i)
    {
        float x = t.x + i/((float)current.period-1)*t.w;
        float y = t.y + t.h*0.5 + current.time[i]*t.h*0.5;
        nk_stroke_line(canvas, prev_x, prev_y, x, y, 2.0f, line_color);
        prev_x = x;
        prev_y = y;
    }

    // Plot frequency domain data
    t.x += spacing;
    nk_fill_rect(canvas, t, 4.0, bg_color);
    for(double x = 0; x < t.w; x += 1.0)
    {
        unsigned lower_bound = x/t.w*(current.time.size()/2-1);
        unsigned upper_bound = (x+1)/t.w*(current.time.size()/2-1);
        float amp = 0.0f;

        for(unsigned i = lower_bound; i <= upper_bound; ++i)
            amp = std::max(current.freq[i*2], amp);

        float height = amp * t.h / current.freq_max_amp;
        struct nk_rect b;
        b.x = t.x+x;
        b.y = t.y+t.h-height;
        b.h = ceil(height);
        b.w = 1.0;

        nk_fill_rect(canvas, b, 0.0, line_color);
    }

    nk_layout_space_end(ctx);
}

void visualizer::start_update(int32_t* samples, unsigned period)
{
    if(
        incoming.valid() &&
        incoming.wait_for(std::chrono::seconds(0)) != std::future_status::ready
    ){
        // Still working on previous, so skip this.
        return;
    }

    incoming = std::async( std::launch::async, [
            &, samples = std::vector<int32_t>(samples, samples+buffer_size),
            period
        ](){
            data new_data;
            new_data.period = period;
            new_data.time.reserve(buffer_size);
            for(int32_t t: samples)
                new_data.time.push_back(t/(double)INT32_MAX);

            analyze_data(new_data);
            return new_data;
        }
    );
}

void visualizer::start_update(const fm_synth& synth)
{
    if(
        incoming.valid() &&
        incoming.wait_for(std::chrono::seconds(0)) != std::future_status::ready
    ){
        // Still working on previous, so skip this.
        return;
    }

    incoming = std::async(std::launch::async, [&, synth](){
        std::vector<int32_t> samples(buffer_size, 0);
        unsigned period = buffer_size > 128 ? 128 : buffer_size;
        fm_synth::state s = synth.start(
            1.0/synth.get_total_carrier_amplitude()
        );
        s.period_num = 4294967296.0/period;
        s.period_denom = 1;
        synth.synthesize(s, samples.data(), buffer_size);

        data new_data;
        new_data.period = period;
        new_data.time.reserve(buffer_size);
        for(int32_t t: samples)
            new_data.time.push_back(t/(double)INT32_MAX);

        analyze_data(new_data);
        return new_data;
    });
}

void visualizer::analyze_data(data& d)
{
    d.freq.resize(d.time.size()*2);
    pffft_transform_ordered(
        setup, d.time.data(), d.freq.data(), NULL, PFFFT_FORWARD
    );
    d.freq_max_amp = 0.0;
    for(unsigned i = 0; i < d.time.size(); ++i)
    {
        std::complex<float> c(d.freq[i*2], d.freq[i*2+1]);
        d.freq[i*2] = logf(std::abs(c));
        if(d.freq[i*2] > d.freq_max_amp) d.freq_max_amp = d.freq[i*2];
        d.freq[i*2+1] = std::arg(c);
    }
}
