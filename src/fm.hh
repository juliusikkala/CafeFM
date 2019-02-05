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
#ifndef CAFEFM_FM_HH
#define CAFEFM_FM_HH
#include "func.hh"
#include "io.hh"
#include "instrument.hh"
#include <cmath>
#include <mutex>
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

class oscillator
{
friend class fm_synth;
public:
    enum func
    {
        SINE = 0,
        SQUARE,
        TRIANGLE,
        SAW,
        NOISE
    };

    struct state
    {
        state();

        int64_t t;
        int64_t output;
    };

    oscillator(
        func type = SINE,
        int64_t period_num = 1,
        int64_t period_denom = 1,
        double amplitude = 1.0,
        double phase_constant = 0.0
    );
    ~oscillator();

    bool operator!=(const oscillator& o) const;

    void set_type(func type);
    func get_type() const;

    void set_amplitude(double amplitude, int64_t denom=65536);
    void set_amplitude(int64_t amp_num, int64_t amp_denom);
    double get_amplitude() const;
    void get_amplitude(int64_t& amp_num, int64_t& amp_denom) const;

    void set_period(uint64_t period_num, uint64_t period_denom);
    void get_period(uint64_t& period_num, uint64_t& period_denom) const;

    void set_period_fine(double fine = 0);
    double get_period_fine() const;

    void set_phase_constant(int64_t offset);
    void set_phase_constant(double offset);
    int64_t get_phase_constant() const;
    double get_phase_constant_double() const;

    std::vector<unsigned>& get_modulators();
    const std::vector<unsigned>& get_modulators() const;

    int64_t value(int64_t t) const;
    void reset(state& s) const;
    void update(
        state& s,
        uint64_t period_num,
        uint64_t period_denom,
        uint64_t phase_offset = 0
    ) const;

protected:
    func type;
    int64_t amp_num, amp_denom;
    int64_t period_num, period_denom;
    int64_t period_fine;
    int64_t phase_constant;

    std::vector<unsigned> modulators;
};

class fm_synth
{
public:
    struct state
    {
        int64_t period_num, period_denom;
        int64_t amp_num, amp_denom;
        std::vector<oscillator::state> states;
    };

    enum modulation_mode
    {
        FREQUENCY = 0,
        PHASE
    };

    fm_synth();

    bool index_compatible(const fm_synth& other) const;

    void set_modulation_mode(modulation_mode mode);
    modulation_mode get_modulation_mode() const;

    std::vector<unsigned>& get_carriers();
    const std::vector<unsigned>& get_carriers() const;

    unsigned get_oscillator_count() const;
    oscillator& get_oscillator(unsigned i);
    const oscillator& get_oscillator(unsigned i) const;
    // Invalidates and updates all oscillator indices after i. May remove
    // several oscillators. States are also invalid afterwards, so restart them.
    void erase_oscillator(unsigned i);
    unsigned add_oscillator(const oscillator& o);
    // Cleans up modulators after dependency changes, ensuring that they are
    // properly formatted.
    void finish_changes();
    // Call this after you are finished modifying the period of any oscillator.
    // finish_changes() also calls this.
    void update_period_lookup();

    double get_total_carrier_amplitude() const;
    void limit_total_carrier_amplitude();

    struct layout
    {
        struct group
        {
            int parent;
            bool empty; // If true, render as empty space with parent width.
            unsigned partition;
            // If oscillators is empty, this group exists as a +-button for the
            // parent.
            std::vector<unsigned> oscillators;
        };
        using layer = std::vector<group>;
        std::vector<layer> layers;
    };

    layout generate_layout();

    state start(double volume = 0.5, int64_t denom = 65536) const;
    void reset(state& s) const;
    // Call this if mode == PHASE
    int64_t step_phase(state& s) const;
    // Call this if mode == FREQUENCY
    int64_t step_frequency(state& s) const;
    void synthesize(state& s, int32_t* samples, unsigned sample_count) const;
    void set_frequency(state& s, double frequency, uint64_t samplerate) const;
    void set_volume(state& s, int64_t volume_num, int64_t volume_denom) const;

    json serialize() const;
    bool deserialize(const json& j);

private:
    using reference_vec = std::vector<std::vector<int>>;

    void erase_invalid_indices();
    reference_vec determine_references();
    void erase_orphans(reference_vec& ref);
    void erase_index(unsigned index, reference_vec* ref = nullptr);
    void sort_oscillators();

    modulation_mode mode;
    std::vector<oscillator> oscillators;
    std::vector<unsigned> carriers;
    std::vector<std::pair<int64_t, int64_t>> period_lookup;
};

class fm_instrument: public instrument
{
public:
    fm_instrument(uint64_t samplerate);

    void set_synth(const fm_synth& s);
    const fm_synth& get_synth();

    void synthesize(int32_t* samples, unsigned sample_count) override;

protected:
    void refresh_voice(voice_id id) override;
    void reset_voice(voice_id id) override;
    void handle_polyphony(unsigned n) override;

private:
    // Double buffered synth changes to avoid skips.
    std::atomic_bool synth_updated;
    unsigned write_index, read_index;
    fm_synth synth[2];
    std::vector<fm_synth::state> states[2];
};

#endif
