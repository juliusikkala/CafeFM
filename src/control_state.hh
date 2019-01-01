#ifndef CAFEFM_CONTROL_CONTEXT_HH
#define CAFEFM_CONTROL_CONTEXT_HH
#include "fm.hh"
#include <set>
#include <map>

class bind;
class control_state
{
public:
    using action_id = uint32_t;

    control_state();

    void set_toggle_state(action_id id, int state);
    int get_toggle_state(action_id id) const;

    void erase_action(action_id id);

    void press_key(action_id id, int semitone);
    void release_key(action_id id);
    bool is_active_key(action_id id) const;

    void set_frequency_expt(action_id id, double freq_expt);
    bool get_frequency_expt(action_id id, double& freq_expt) const;

    void set_volume_mul(action_id id, double volume_mul);
    bool get_volume_mul(action_id id, double& volume_mul) const;

    void set_period_expt(
        unsigned modulator_index, action_id id, double period_expt
    );
    bool get_period_expt(
        unsigned modulator_index, action_id id, double& period_expt
    ) const;

    void set_amplitude_mul(
        unsigned modulator_index, action_id id, double amplitude_mul
    );
    bool get_amplitude_mul(
        unsigned modulator_index, action_id id, double& amplitude_mul
    ) const;

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

    std::vector<std::pair<action_id, int /* semitone */>> press_queue;
    std::vector<action_id> release_queue;

    // TODO: These should be vectors for performance reasons. They are rarely
    // indexed but often iterated.
    std::map<instrument::voice_id, action_id> pressed_keys;

    std::map<action_id, int> toggle_state;

    std::map<action_id, double> freq_expt;
    std::map<action_id, double> volume_mul;

    struct oscillator_mod
    {
        std::map<action_id, double> period_expt;
        std::map<action_id, double> amplitude_mul;
    } osc[3];
};

#endif
