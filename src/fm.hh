#ifndef CAFEFM_FM_HH
#define CAFEFM_FM_HH
#include "func.hh"
#include "io.hh"
#include "instrument.hh"
#include <cmath>
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
        SAW
    };

    struct state
    {
        state();

        int64_t t;
        int64_t output;
    };

    oscillator(
        func type = SINE,
        double period = 1.0,
        double amplitude = 1.0,
        double phase_constant = 0.0
    );
    oscillator(
        func type,
        double frequency,
        double volume,
        uint64_t samplerate
    );
    ~oscillator();

    bool operator!=(const oscillator& o) const;

    void set_type(func type);
    func get_type() const;

    void set_amplitude(double amplitude, int64_t denom=65536);
    void set_amplitude(int64_t amp_num, int64_t amp_denom);
    double get_amplitude() const;
    void get_amplitude(int64_t& amp_num, int64_t& amp_denom) const;

    void set_period_fract(uint64_t period_num, uint64_t period_denom);
    void set_period(double period, uint64_t denom=65536);
    double get_period() const;
    void get_period(uint64_t& period_num, uint64_t& period_denom) const;
    void set_frequency(double freq, uint64_t samplerate);

    void set_phase_constant(int64_t offset);
    void set_phase_constant(double offset);
    int64_t get_phase_constant() const;
    double get_phase_constant_double() const;

    std::vector<unsigned>& get_modulators();
    const std::vector<unsigned>& get_modulators() const;

    int64_t value(int64_t t) const;
    void reset(state& s) const;
    void update(state& s, uint64_t period_num, uint64_t period_denom) const;

protected:
    func type;
    int64_t amp_num, amp_denom;
    uint64_t period_num, period_denom;
    int64_t phase_constant;

    std::vector<unsigned> modulators;
};

class fm_synth
{
public:
    struct state
    {
        oscillator carrier;
        std::vector<oscillator::state> states;
    };

    fm_synth();

    bool index_compatible(const fm_synth& other) const;

    void set_carrier_type(oscillator::func carrier_type);
    oscillator::func get_carrier_type() const;

    std::vector<unsigned>& get_carrier_modulators();
    const std::vector<unsigned>& get_carrier_modulators() const;

    unsigned get_modulator_count() const;
    oscillator& get_modulator(unsigned i);
    const oscillator& get_modulator(unsigned i) const;
    // Invalidates and updates all modulator indices after i. May remove several
    // modulators. States are also invalid afterwards, so restart them.
    void erase_modulator(unsigned i);
    unsigned add_modulator(const oscillator& o);
    // Cleans up modulators after changes, ensuring that they are properly
    // formatted.
    void finish_changes();

    state start(
        double frequency = 440.0,
        double volume = 0.5,
        uint64_t samplerate = 44100
    ) const;
    void reset(state& s) const;
    int64_t step(state& s) const;
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
    void sort_oscillator_modulators();

    std::vector<oscillator> modulators;
    oscillator::func carrier_type;
    std::vector<unsigned> carrier_modulators;
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
    fm_synth synth;
    std::vector<fm_synth::state> states;
};

#endif
