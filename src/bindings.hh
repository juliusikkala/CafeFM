#ifndef CAFEFM_BINDINGS_HH
#define CAFEFM_BINDINGS_HH
#include "control_context.hh"
#include "io.hh"
#include <vector>
#include <string>

struct bind
{
    bind();

    json serialize() const;
    bool deserialize(const json& j);

    enum control
    {
        UNBOUND = 0,
        BUTTON_PRESS,
        BUTTON_TOGGLE,
        AXIS_1D_CONTINUOUS,
        AXIS_1D_THRESHOLD,
        AXIS_1D_THRESHOLD_TOGGLE
    } control;

    // Do not modify this manually, it's set by bindings::create_new_bind and
    // used for internal bookkeeping.
    control_context::action_id id;

    union
    {
        // BUTTON_PRESS, BUTTON_TOGGLE
        struct
        {
            int index;
            bool active_pressed;
        } button;

        // AXIS_1D_CONTINUOUS, AXIS_1D_THRESHOLD, AXIS_1D_THRESHOLD_TOGGLE
        struct
        {
            int index;
            bool invert;
            float threshold; // unused for continuous
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

        // VOLUME_MUL (discrete jumps from min to max, must be positive)
        struct
        {
            double min_mul, max_mul;
        } volume;

        // PERIOD_EXPT (discrete jumps to max, can be negative)
        struct
        {
            unsigned modulator_index;
            double max_expt;
        } period;

        // AMPLITUDE_MUL (discrete jumps from min to max, must be positive)
        struct
        {
            unsigned modulator_index;
            double min_mul, max_mul;
        } amplitude;
    };
};

class controller;
class control_context;

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

    void set_target_device(controller* c);

    void set_target_device_type(const std::string& type);
    std::string get_target_device_type() const;

    void set_target_device_name(const std::string& name);
    std::string get_target_device_name() const;

    // 0 - Bindings were made for this controller
    // 1 - Bindings are for this device type and indices are within range
    // 2 - Indices are within range.
    // 3 - Bindings are for this device type but some indices are out of range
    // 4 - Bindings are not for this device type and some indices are out of
    //     range.
    unsigned rate_compatibility(controller* c) const;

    // Applies controller inputs to control context according to the bindings.
    void act(
        control_context& ctx,
        controller* c,
        int axis_1d_index,
        int axis_2d_index,
        int button_index
    );

    bind& create_new_bind();
    bind& get_bind(unsigned i);
    const bind& get_bind(unsigned i) const;
    void erase_bind(unsigned i);
    size_t bind_count() const;

    json serialize() const;
    bool deserialize(const json& j);
    void clear();

private:
    bool write_lock;
    std::string name;
    std::string device_type, device_name;
    std::vector<bind> binds;
    control_context::action_id id_counter;
};

#endif
