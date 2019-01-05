#ifndef CAFEFM_CONTROL_CONTEXT_HH
#define CAFEFM_CONTROL_CONTEXT_HH
#include "fm.hh"
#include "instrument_state.hh"
#include <set>
#include <map>

class bind;
class controller;
class bindings;
class control_state
{
public:
    using action_id = uint32_t;

    control_state();

    void set_toggle_state(action_id id, int state);
    int get_toggle_state(action_id id) const;

    void set_cumulation_speed(action_id id, double speed);
    void clear_cumulation(action_id id);
    double get_cumulation(action_id id);

    void set_stacking(action_id id, int s);
    int get_stacking(action_id id) const;

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

    void set_envelope_adjust(unsigned which, action_id id, double mul);
    bool get_envelope_adjust(unsigned which, action_id id, double& mul) const;

    void reset();
    void update(bindings& b, unsigned dt);

    double total_freq_mul() const;
    double total_volume_mul() const;
    double total_period_mul(unsigned oscillator_index) const;
    double total_amp_mul(unsigned oscillator_index) const;
    double total_envelope_adjust(unsigned which) const;

    // Applies alterations by actions to synth and modulators.
    void apply(
        fm_instrument& ins,
        double src_volume,
        instrument_state& ins_state
    );

private:
    std::vector<std::pair<action_id, int /* semitone */>> press_queue;
    std::vector<action_id> release_queue;

    // TODO: These should be vectors for performance reasons. They are rarely
    // indexed but often iterated.
    std::map<instrument::voice_id, action_id> pressed_keys;

    std::map<action_id, int> toggle_state;
    std::map<action_id, int> stacking;
    std::map<
        action_id,
        std::pair<double /* Cumulation */, double /* Speed */>
    > cumulative_state;

    std::map<action_id, double> freq_expt;
    std::map<action_id, double> volume_mul;

    struct oscillator_mod
    {
        std::map<action_id, double> period_expt;
        std::map<action_id, double> amplitude_mul;
    };
    std::vector<oscillator_mod> osc;

    struct envelope_mod
    {
        std::map<action_id, double> mul;
    } env[4];
};

#endif
