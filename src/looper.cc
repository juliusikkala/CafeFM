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
#include "looper.hh"
#include "instrument.hh"
#include "helpers.hh"
#include <algorithm>
#include <cmath>

looper::looper(uint64_t samplerate)
:   samplerate(samplerate), ins(nullptr), beat_length(0), loop_t(0),
    volume_denom(65536)
{
    set_max_volume_skip();
    set_loop_bpm();
    reset_loops();
}

void looper::set_instrument(instrument* ins)
{
    this->ins = ins;
}

void looper::reset_loops(size_t max_count, double max_loop_length)
{
    unsigned loop_size = max_loop_length * samplerate;

    loop_samples.resize(max_count*loop_size);
    memset(loop_samples.data(), 0, loop_samples.size()*sizeof(int32_t));

    loops.resize(max_count);
    for(size_t i = 0; i < loops.size(); ++i)
    {
        loop& l = loops[i];
        l.state = UNUSED;
        l.target_volume_num = volume_denom;
        l.volume_num = 0;
        l.start_t = 0;
        l.relative_start_t = 0;
        l.length = 0;
        l.sample_count = 0;
        l.record_stop_timer = 0;
        l.samples = loop_samples.data() + i * loop_size;
    }

    loop_t = 0;
    selected_loop = 0;
}

unsigned looper::get_loop_count() const
{
    return loops.size();
}

void looper::set_loop_bpm(double bpm)
{
    beat_length = samplerate * 60.0 / bpm;
}

double looper::get_loop_bpm() const
{
    return samplerate * 60.0 / beat_length;
}

double looper::get_loop_beat_index() const
{
    return loop_t/(double)beat_length;
}

void looper::set_loop_volume(unsigned loop_index, double volume)
{
    loops[loop_index].target_volume_num = volume*volume_denom;
}

double looper::get_loop_volume(unsigned loop_index) const
{
    return loops[loop_index].target_volume_num/(double)volume_denom;
}

void looper::record_loop(unsigned loop_index)
{
    clear_loop(loop_index);
    loop& l = loops[loop_index];
    l.state = RECORDING;
    l.start_t = loop_t;
    l.relative_start_t = loop_t;
    l.length = 0;
    l.sample_count = 0;
}

void looper::finish_loop(unsigned loop_index)
{
    if(ins) ins->release_all_voices();
    loop& l = loops[loop_index];
    l.length = (l.sample_count + 3*beat_length/4) / beat_length * beat_length;
    if(l.length == 0) l.length = beat_length;
    l.record_stop_timer = ins ? ins->get_envelope().release_length : 0;
    l.state = PLAYING;
}

void looper::play_loop(unsigned loop_index, bool play)
{
    loops[loop_index].state = play ? PLAYING : MUTED;
}

void looper::clear_loop(unsigned loop_index)
{
    unsigned loop_size = loop_samples.size()/loops.size();
    loop& l = loops[loop_index];
    l.state = UNUSED;
    l.target_volume_num = volume_denom;
    l.volume_num = 0;
    l.start_t = 0;
    l.relative_start_t = 0;
    l.length = 0;
    l.sample_count = 0;
    l.record_stop_timer = 0;
    memset(l.samples, 0, sizeof(int32_t)*loop_size);
}

void looper::clear_all_loops()
{
    for(size_t i = 0; i < loops.size(); ++i) clear_loop(i);
}

looper::loop_state looper::get_loop_state(unsigned loop_index) const
{
    return loops[loop_index].state;
}

void looper::set_loop_length(unsigned loop_index, double length)
{
    loop& l = loops[loop_index];
    int64_t delay = l.start_t - l.relative_start_t;
    l.start_t = loop_t - ((loop_t - l.start_t) % l.length);
    l.relative_start_t = l.start_t - delay;
    l.length = round(length * beat_length);
    if(l.length == 0) l.length = 1;
}

double looper::get_loop_length(unsigned loop_index) const
{
    return (loops[loop_index].state == RECORDING ?
        loops[loop_index].sample_count :
        loops[loop_index].length)/(double)beat_length;
}

void looper::set_loop_delay(unsigned loop_index, double delay)
{
    loop& l = loops[loop_index];
    l.start_t = l.relative_start_t + (int64_t)(delay * beat_length);
}

double looper::get_loop_delay(unsigned loop_index) const
{
    const loop& l = loops[loop_index];
    return (l.start_t-l.relative_start_t)/(double)beat_length;
}

void looper::apply(int32_t* o, unsigned long framecount)
{
    // Handle loop recording
    for(unsigned j = 0; j < loops.size(); ++j)
    {
        loop& l = loops[j];
        if(l.state != RECORDING && l.record_stop_timer <= 0) continue;

        unsigned max_samples = loop_samples.size() / loops.size();
        unsigned t = loop_t - l.start_t;
        unsigned max_samples_left = max_samples - t;
        unsigned write_length = framecount;

        if(max_samples_left <= framecount)
        {
            l.length = (l.sample_count + max_samples_left)
                / beat_length * beat_length;
            l.record_stop_timer = 0;
            l.state = PLAYING;
            write_length = max_samples_left;
        }

        for(unsigned i = 0; i < write_length; ++i, ++t)
        {
            l.sample_count++;
            l.samples[t] = o[i];
        }
    }

    // Handle loop playback
    for(loop& l: loops)
    {
        if(l.volume_num == 0 && l.state != PLAYING) continue;

        unsigned t = (loop_t - l.start_t) % l.length;

        // TODO: Optimize & clarify
        // Don't take samples into account if still recording them.
        if(l.record_stop_timer <= 0)
        {
            for(unsigned i = 0; i < framecount; ++i, ++t)
            {
                update_loop_volume(l);
                if(t >= l.length) t -= l.length;
                int64_t sample = 0;
                // Key releases may overlap with the start of the loop. Also,
                // the user may have adjusted the loop length such that it
                // overlaps with itself.
                for(unsigned w = 0; t + w < l.sample_count; w += l.length)
                    sample += l.samples[t+w];
                o[i] += l.volume_num*sample/volume_denom;
            }
        }
    }

    // Update timers
    for(loop& l: loops)
    {
        if(l.record_stop_timer > 0) l.record_stop_timer -= framecount;
    }
    loop_t += framecount;
}

void looper::set_max_volume_skip(double skip)
{
    max_volume_skip = skip * volume_denom;
}

void looper::set_selected_loop(int selected_loop)
{
    while(selected_loop < 0) selected_loop += loops.size();
    selected_loop %= loops.size();
    this->selected_loop = selected_loop;
}

int looper::get_selected_loop() const
{
    return selected_loop;
}

void looper::update_loop_volume(loop& l)
{
    uint64_t target = l.state == PLAYING ? l.target_volume_num : 0;

    int64_t skip_size = target - l.volume_num;
    if(skip_size < -max_volume_skip) skip_size = -max_volume_skip;
    if(skip_size > max_volume_skip) skip_size = max_volume_skip;
    l.volume_num += skip_size;
}
