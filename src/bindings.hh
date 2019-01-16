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
#ifndef CAFEFM_BINDINGS_HH
#define CAFEFM_BINDINGS_HH
#include "control_state.hh"
#include "io.hh"
#include <vector>
#include <string>

class controller;
class control_state;
class looper;

struct bind
{
    json serialize() const;
    bool deserialize(const json& j);

    // Do not modify this manually, it's set by bindings::create_new_bind and
    // used for internal bookkeeping.
    control_state::action_id id;

    // Used for implementing bind assignment. This is not saved to the bindings
    // file.
    bool wait_assign;

    enum control
    {
        UNBOUND = 0,
        BUTTON_PRESS,
        AXIS_1D_CONTINUOUS,
        AXIS_1D_THRESHOLD
    } control;

    // Ignored for AXIS_1D_CONTINUOUS
    bool toggle;
    bool cumulative;
    // If set, toggle and cumulative should be false.
    bool stacking;

    union
    {
        // BUTTON_PRESS
        struct
        {
            int index;
            unsigned active_state;
        } button;

        // AXIS_1D_CONTINUOUS, AXIS_1D_THRESHOLD
        struct
        {
            int index;
            bool invert;
            double threshold; // deadzone from offset for continuous
            double origin; // Continuous only: point considered as zero.
        } axis_1d;
    };

    enum action
    {
        KEY = 0, // Discrete control only
        FREQUENCY_EXPT,
        VOLUME_MUL,
        PERIOD_EXPT,
        AMPLITUDE_MUL,
        ENVELOPE_ADJUST,
        LOOP_CONTROL
    } action;

    enum loop_control
    {
        LOOP_RECORD = 0,
        LOOP_CLEAR,
        LOOP_MUTE
    };

    union
    {
        // KEY
        int key_semitone;

        // FREQUENCY_EXPT (discrete jumps to max, can be negative.)
        struct
        {
            double max_expt;
        } frequency;

        // VOLUME_MUL (discrete jumps to max, must be positive)
        struct
        {
            double max_mul;
        } volume;

        // PERIOD_EXPT (discrete jumps to max, can be negative)
        struct
        {
            unsigned modulator_index;
            double max_expt;
        } period;

        // AMPLITUDE_MUL (discrete jumps to max, must be positive)
        struct
        {
            unsigned modulator_index;
            double max_mul;
        } amplitude;

        // ENVELOPE_ADJUST (discrete jumps to max, must be positive)
        struct
        {
            // Which envelope part?
            // 0 - attack
            // 1 - decay
            // 2 - sustain
            // 3 - release
            unsigned which;
            double max_mul;
        } envelope;

        // LOOP_CONTROL (discrete only)
        struct
        {
            // -1 for next/prev, -2 for all
            int index;
            loop_control control;
        } loop;
    };

    bind(enum action a = KEY);

    bool triggered(
        int axis_1d_index,
        int axis_2d_index,
        int button_index
    ) const;

    double input_value(const controller* c, bool* is_signed = nullptr) const;

    double get_value(const control_state& ctx, const controller* c) const;
    // Returns false if should be skipped
    bool update_value(
        control_state& ctx,
        const controller* c,
        double& value
    ) const;
    double normalize(const controller* c, double value) const;
};

class bindings
{
public:
    bindings();

    // Note that these don't actually block anything; that must be done in the
    // GUI.
    void set_write_lock(bool lock);
    bool is_write_locked() const;

    void set_name(const std::string& name);
    std::string get_name() const;

    void set_path(const fs::path& path);
    fs::path get_path() const;

    void set_target_device(controller* c);

    void set_target_device_type(const std::string& type);
    std::string get_target_device_type() const;

    void set_target_device_name(const std::string& name);
    std::string get_target_device_name() const;

    // 0 - Bindings were made for this controller
    // 1 - Bindings are for this device type and indices are within range
    // 2 - Bindings are for this device type but some indices are out of range
    // 3 - Indices are within range.
    // 4 - Bindings are not for this device type and some indices are out of
    //     range.
    unsigned rate_compatibility(controller* c) const;

    // Called by control state whenever cumulative values have changed.
    void cumulative_update(control_state& state);

    // Applies controller inputs to control state and loops according to the
    // bindings.
    void act(
        controller* c,
        control_state& state,
        looper* loop,
        int axis_1d_index,
        int axis_2d_index,
        int button_index
    );

    bind& create_new_bind(enum bind::action action = bind::KEY);
    bind& get_bind(unsigned i);
    const bind& get_bind(unsigned i) const;
    const std::vector<bind>& get_binds() const;
    // 1 - move up
    // 0 - keep
    // -1 - move down
    // -2 - remove
    void move_bind(
        unsigned i,
        int movement,
        control_state& state,
        bool same_action = true
    );
    void erase_bind(unsigned i, control_state& state);
    size_t bind_count() const;

    json serialize() const;
    bool deserialize(const json& j);
    void clear();

private:
    int handle_loop_event(
        looper& loop,
        bind::loop_control control,
        int index,
        double value
    );

    void handle_action(
        control_state& state,
        looper* loop,
        const bind& b,
        double input_value
    );

    bool write_lock;
    std::string name;
    fs::path path;
    std::string device_type, device_name;
    std::vector<bind> binds;
    control_state::action_id id_counter;
};

#endif
