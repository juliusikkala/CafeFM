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
#ifndef CAFEFM_INSTRUMENT_HH
#define CAFEFM_INSTRUMENT_HH
#include <vector>
#include <cstdint>
#include <memory>
#include "filter.hh"

struct envelope
{
    void set_volume(
        double peak_volume,
        double sustain_volume,
        int64_t denom = 1<<20
    );

    void set_curve(
        double attack_length,
        double decay_length,
        double release_length,
        uint64_t samplerate
    );

    envelope convert(
        uint64_t cur_samplerate,
        uint64_t new_samplerate
    ) const;

    bool operator==(const envelope& other) const;

    int64_t peak_volume_num;
    int64_t sustain_volume_num; // Set to 0 for no sustain
    int64_t volume_denom;

    uint64_t attack_length;
    uint64_t decay_length;
    uint64_t release_length;
};

class instrument
{
public:
    instrument(uint64_t samplerate);
    virtual ~instrument();

    using voice_id = unsigned;

    void set_tuning(double base_frequency = 440.0);
    double get_tuning() const;

    uint64_t get_samplerate() const;

    voice_id press_voice(int semitone);
    void press_voice(voice_id id, int semitone, double volume = 1.0);
    void set_voice_volume(voice_id id, double volume = 1.0);
    void release_voice(voice_id id);
    void release_all_voices();
    // Makes sure all updates are applied to voices.
    void refresh_all_voices();

    void set_polyphony(unsigned n = 16);
    unsigned get_polyphony() const;

    void set_envelope(const envelope& adsr);
    envelope get_envelope() const;

    void set_volume(double volume);
    void set_max_safe_volume();
    double get_volume() const;

    // How much volume can change in a second.
    void set_max_volume_skip(double max_volume_skip);

    void set_filter(filter&& f);
    void clear_filter();

    void copy_state(const instrument& other);

    virtual void synthesize(int32_t* samples, unsigned sample_count) = 0;

protected:
    struct voice
    {
        bool enabled;
        bool pressed;
        uint64_t press_timer;
        uint64_t release_timer;
        int semitone;
        int64_t volume_num;
        int64_t volume; // Used for limiting volume jumps
    };

    double get_frequency(voice_id id) const;
    void get_voice_volume(voice_id id, int64_t& num, int64_t& denom);
    void step_voice(voice_id id);
    void apply_filter(int32_t* samples, unsigned sample_count);

    virtual void refresh_voice(voice_id id) = 0;
    virtual void reset_voice(voice_id id) = 0;
    virtual void handle_polyphony(unsigned n) = 0;

private:
    // If this is too slow, consider generating a table from the envelope
    void update_voice_volume(voice& v);

    std::vector<voice> voices;
    envelope adsr;
    double base_frequency;
    int64_t volume_num, volume_denom;
    int64_t max_volume_skip;
    uint64_t samplerate;

    std::unique_ptr<filter> used_filter;
};

#endif
