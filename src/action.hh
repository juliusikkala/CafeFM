#ifndef CAFEFM_ACTION_HH
#define CAFEFM_ACTION_HH
#include "fm.hh"
#include <set>
#include <map>

struct action
{
    using source_id = uint32_t;
    // Used to detect continuity. This should be set by the binding generating
    // the action.
    source_id id;

    enum type
    {
        PRESS_KEY,
        RELEASE_KEY,
        SET_FREQUENCY_EXPT,
        SET_VOLUME_MUL,
        SET_PERIOD_EXPT,
        SET_AMPLITUDE_MUL
    } type;

    union
    {
        // PRESS_KEY
        int semitone;

        // SET_FREQUENCY_MUL
        double freq_expt; // In semitones, adjusts base frequency of the synth

        // SET_VOLUME_MUL
        double volume_mul;
        
        // SET_PERIOD_MUL
        struct
        {
            unsigned modulator_index;
            double expt; // In semitones, adjusts oscillator period directly
        } period;

        // SET_AMPLITUDE_MUL
        struct
        {
            unsigned modulator_index;
            double mul;
        } amplitude;
    };
};

class action_context
{
public:
    action_context();

    void queue(const action& a);
    void reset();

    double total_freq_mul() const;
    double total_volume_mul() const;
    double total_period_mul(unsigned oscillator_index) const;
    double total_amp_mul(unsigned oscillator_index) const;

    // Applies alterations by actions to synth and modulators.
    void apply(
        basic_fm_synth& synth,
        double src_volume,
        const std::vector<dynamic_oscillator>& src_modulators
    );

private:
    // Stored here to avoid extra allocations
    std::vector<dynamic_oscillator> dst_modulators;

    std::vector<std::pair<action::source_id, int /* semitone */>> press_queue;
    std::vector<action::source_id> release_queue;

    // TODO: These could be vectors for performance reasons. They are rarely
    // indexed but often iterated.
    std::map<instrument::voice_id, action::source_id> pressed_keys;

    std::map<action::source_id, double> freq_expt;
    std::map<action::source_id, double> volume_mul;

    struct oscillator_mod
    {
        std::map<action::source_id, double> period_expt;
        std::map<action::source_id, double> amplitude_mul;
    } osc[3];
};

#endif
