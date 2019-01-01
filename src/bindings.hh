#ifndef CAFEFM_BINDINGS_HH
#define CAFEFM_BINDINGS_HH
#include "control_state.hh"
#include "io.hh"
#include <vector>
#include <string>

class controller;
class control_state;

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
            double threshold; // unused for continuous
        } axis_1d;
    };

    enum action
    {
        KEY, // Discrete control only
        FREQUENCY_EXPT,
        VOLUME_MUL,
        PERIOD_EXPT,
        AMPLITUDE_MUL
    } action;

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

    // Applies controller inputs to control state according to the bindings.
    void act(
        control_state& state,
        controller* c,
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
    void handle_action(control_state& state, const bind& b, double input_value);

    bool write_lock;
    std::string name;
    fs::path path;
    std::string device_type, device_name;
    std::vector<bind> binds;
    control_state::action_id id_counter;
};

#endif
