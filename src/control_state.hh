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
    using controller_id = uint32_t;
    using full_id = uint64_t;
    static full_id create_id(controller_id cid, action_id aid);
    static void split_id(full_id id, controller_id& cid, action_id& aid);

    control_state();

    void set_threshold_state(controller_id cid, action_id aid, int active);
    int get_threshold_state(controller_id cid, action_id aid) const;

    void set_toggle_state(controller_id cid, action_id aid, int state);
    int get_toggle_state(controller_id cid, action_id aid) const;

    void set_cumulation_speed(controller_id cid, action_id aid, double speed);
    void clear_cumulation(controller_id cid, action_id aid);
    double get_cumulation(controller_id cid, action_id aid);

    void set_stacking(controller_id cid, action_id aid, int s);
    int get_stacking(controller_id cid, action_id aid) const;

    void erase_action(controller_id cid, action_id aid);

    void press_key(
        controller_id cid, action_id aid, int semitone, double volume
    );
    void set_key_volume(controller_id cid, action_id aid, double volume);
    void release_key(controller_id cid, action_id aid);
    bool is_active_key(controller_id cid, action_id aid) const;

    void set_frequency_expt(controller_id cid, action_id aid, double freq_expt);
    bool get_frequency_expt(
        controller_id cid, action_id aid, double& freq_expt
    ) const;

    void set_volume_mul(controller_id cid, action_id aid, double volume_mul);
    bool get_volume_mul(controller_id cid, action_id aid, double& volume_mul) const;

    void set_period_fine(
        unsigned modulator_index,
        controller_id cid,
        action_id aid,
        double period_fine
    );
    bool get_period_fine(
        unsigned modulator_index,
        controller_id cid,
        action_id aid,
        double& period_fine
    ) const;

    void set_amplitude_mul(
        unsigned modulator_index,
        controller_id cid,
        action_id aid,
        double amplitude_mul
    );
    bool get_amplitude_mul(
        unsigned modulator_index,
        controller_id cid,
        action_id aid,
        double& amplitude_mul
    ) const;

    void set_envelope_adjust(
        unsigned which, controller_id cid, action_id id, double mul
    );
    bool get_envelope_adjust(
        unsigned which, controller_id cid, action_id id, double& mul
    ) const;

    void reset();
    void reset(controller_id cid);
    void update(controller_id cid, bindings& b, unsigned dt);

    double total_freq_mul(double base_freq = 440.0) const;
    double total_volume_mul() const;
    double total_period_fine(unsigned oscillator_index) const;
    double total_amp_mul(unsigned oscillator_index) const;
    double total_envelope_adjust(unsigned which) const;

    // Applies alterations by actions to synth and modulators.
    void apply(
        fm_instrument& ins,
        double src_volume,
        instrument_state& ins_state
    );

private:
    struct key_data
    {
        full_id id;
        int semitone;
        double volume;
    };
    std::vector<key_data> press_queue;
    std::vector<full_id> release_queue;

    // TODO: These should be vectors for performance reasons. They are rarely
    // indexed but often iterated.
    std::map<instrument::voice_id, key_data> pressed_keys;

    std::map<full_id, int> threshold_state;
    std::map<full_id, int> toggle_state;
    std::map<full_id, int> stacking;
    std::map<
        full_id,
        std::pair<double /* Cumulation */, double /* Speed */>
    > cumulative_state;

    std::map<full_id, double> freq_expt;
    std::map<full_id, double> volume_mul;

    struct oscillator_mod
    {
        std::map<full_id, double> period_fine;
        std::map<full_id, double> amplitude_mul;
    };
    std::vector<oscillator_mod> osc;

    struct envelope_mod
    {
        std::map<full_id, double> mul;
    } env[4];
};

#endif
