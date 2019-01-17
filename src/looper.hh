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
#ifndef CAFEFM_LOOPER_HH
#define CAFEFM_LOOPER_HH
#include <cstdint>
#include <cstddef>
#include <vector>

class instrument;
class looper
{
public:
    explicit looper(uint64_t samplerate = 44100);
    looper(const looper& other) = delete;

    // Don't call this manually, it's done from audio_output.
    void set_instrument(instrument* ins);

    void reset_loops(size_t max_count = 8, double max_loop_length = 30);
    unsigned get_loop_count() const;

    void set_loop_bpm(double bpm = 120);
    double get_loop_bpm() const;

    // Used for visualizing BPM.
    double get_loop_beat_index() const;
    void set_loop_volume(unsigned loop_index, double volume);
    double get_loop_volume(unsigned loop_index) const;

    void set_record_on_sound(bool start_on_sound);
    void record_loop(unsigned loop_index);
    void finish_loop(unsigned loop_index);
    void play_loop(unsigned loop_index, bool play = true);

    void clear_loop(unsigned loop_index);
    void clear_all_loops();

    enum loop_state
    {
        UNUSED = 0,
        MUTED,
        PLAYING,
        RECORDING
    };

    loop_state get_loop_state(unsigned loop_index) const;

    // Length is in beats according to BPM.
    void set_loop_length(unsigned loop_index, double length);
    double get_loop_length(unsigned loop_index) const;

    void set_loop_delay(unsigned loop_index, double delay);
    double get_loop_delay(unsigned loop_index) const;

    void apply(int32_t* o, unsigned long framecount);

    void set_max_volume_skip(double skip = 0.0001);

    // Used for tracking selected loop in bindings
    void set_selected_loop(int selected_loop);
    int get_selected_loop() const;

private:
    struct loop
    {
        loop_state state;

        int64_t target_volume_num;
        int64_t volume_num;
        int64_t start_t;
        int64_t relative_start_t; // Comparison point for delay.
        uint64_t length;
        int64_t record_stop_timer;
        bool record_on_sound;
        size_t sample_count;
        int32_t* samples;
    };

    void update_loop_volume(loop& l);

    uint64_t samplerate;
    instrument* ins;

    uint64_t beat_length;
    int64_t loop_t;
    int64_t max_volume_skip;
    int64_t volume_denom;
    std::vector<int32_t> loop_samples;
    std::vector<loop> loops;

    int selected_loop;
    bool record_on_sound;
};

#endif
