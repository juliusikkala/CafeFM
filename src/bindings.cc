#include "bindings.hh"
#include "controller/controller.hh"
#include <stdexcept>

namespace
{
    double lerp(double a, double b, double t) { return (1 - t) * a + t * b; }
}

bind::bind()
: control(UNBOUND), id(0), action(KEY), key_semitone(0) { }

bindings::bindings() { clear(); }

void bindings::set_name(const std::string& name) { this->name = name; }
std::string bindings::get_name() const { return name; }

void bindings::set_target_device(controller* c)
{
    if(!c) return;
    set_target_device_type(c->get_type_name());
    set_target_device_name(c->get_device_name());
}

void bindings::set_target_device_type(const std::string& type)
{
    device_type = type;
}

std::string bindings::get_target_device_type() const { return device_type; }

void bindings::set_target_device_name(const std::string& name)
{
    device_name = name;
}

std::string bindings::get_target_device_name() const
{
    return device_name;
}

unsigned bindings::rate_compatibility(controller* c) const
{
    if(!c) return 4;
    int axis_1d_count = c->get_axis_1d_count();
    int button_count = c->get_button_count();

    bool type_match = c->get_type_name() == device_type;
    bool name_match = c->get_device_name() == device_name;
    bool index_match = true;

    for(const bind& b: binds)
    {
        switch(b.control)
        {
        case bind::BUTTON_PRESS:
        case bind::BUTTON_TOGGLE:
            if(b.button.index >= button_count) index_match = false;
            break;
        case bind::AXIS_1D_CONTINUOUS:
        case bind::AXIS_1D_THRESHOLD:
        case bind::AXIS_1D_THRESHOLD_TOGGLE:
            if(b.axis_1d.index >= axis_1d_count) index_match = false;
            break;
        default:
            break;
        }
    }

    if(type_match)
    {
        if(name_match && index_match) return 0;
        else if(index_match) return 1;
        else return 3;
    }
    else if(index_match) return 2;
    else return 4;
}

void bindings::act(
    control_context& ctx,
    controller* c,
    int button_index,
    int axis_1d_index,
    int axis_2d_index
){
    for(const bind& b: binds)
    {
        // Skip unbound controls
        if(b.control == bind::UNBOUND) continue;

        // Get control info and check if triggered
        bool discrete = false;
        bool toggle = false;
        bool triggered = false;

        switch(b.control)
        {
        case bind::BUTTON_TOGGLE:
            toggle = true;
            /* Fallthrough intentional */
        case bind::BUTTON_PRESS:
            discrete = true;
            triggered = button_index >= 0 && button_index == b.button.index;
            break;

        case bind::AXIS_1D_CONTINUOUS:
            triggered = axis_1d_index >= 0 && axis_1d_index == b.axis_1d.index;
            break;

        case bind::AXIS_1D_THRESHOLD_TOGGLE:
            toggle = true;
            /* Fallthrough intentional */
        case bind::AXIS_1D_THRESHOLD:
            discrete = true;
            triggered = axis_1d_index >= 0 && axis_1d_index == b.axis_1d.index;
            break;

        default:
            throw std::runtime_error(
                "Unknown bind control " + std::to_string(b.control)
            );
        }
        // Skip if this bind wasn't triggered
        if(!triggered) continue;

        // Figure out input state.
        bool button_state = false;
        float axis_state = 0.0f;
        float axis_state_normalized = 0.0f;

        switch(b.control)
        {
        case bind::BUTTON_PRESS:
        case bind::BUTTON_TOGGLE:
            button_state = c->get_button_state(button_index);
            if(!b.button.active_pressed) button_state = !button_state;
            break;
        case bind::AXIS_1D_CONTINUOUS:
        case bind::AXIS_1D_THRESHOLD:
        case bind::AXIS_1D_THRESHOLD_TOGGLE:
            {
                axis_1d ax = c->get_axis_1d_state(axis_1d_index);
                axis_state = ax.value;
                axis_state_normalized = axis_state;

                if(ax.is_signed)
                    axis_state_normalized = (axis_state_normalized + 1.0)*0.5;

                if(b.axis_1d.invert)
                {
                    if(ax.is_signed || ax.is_limited) axis_state = -axis_state;
                    else axis_state = 1.0 - axis_state;
                }
            }
            break;
        default:
            break;
        }

        // Determine state of discrete input
        bool discrete_state = button_state;
        if(
            b.control == bind::AXIS_1D_THRESHOLD ||
            b.control == bind::AXIS_1D_THRESHOLD_TOGGLE
        ) discrete_state = axis_state > b.axis_1d.threshold;

        // Handle toggling logic if necessary
        if(toggle)
        {
            if(!discrete_state) continue;
            bool active = ctx.is_active(b);
            discrete_state = !active;
        }

        // Finally perform actual action.
        switch(b.action)
        {
        case bind::KEY:
            if(!discrete)
                throw std::runtime_error("bind::KEY action must be discrete!");

            if(discrete_state) ctx.press_key(b.id, b.key_semitone);
            else ctx.release_key(b.id);
            break;
        case bind::FREQUENCY_EXPT:
            {
                float expt = 0.0;
                if(discrete) expt = discrete_state ? b.frequency.max_expt : 0.0;
                else expt = b.frequency.max_expt * axis_state;
                ctx.set_frequency_expt(b.id, expt);
            }
            break;
        case bind::VOLUME_MUL:
            {
                float mul = 0.0;
                if(discrete)
                    mul = discrete_state ? b.volume.max_mul : b.volume.min_mul;
                else mul = lerp(
                    b.volume.min_mul,
                    b.volume.max_mul,
                    axis_state_normalized
                );
                ctx.set_volume_mul(b.id, mul);
            }
            break;
        case bind::PERIOD_EXPT:
            {
                float expt = 0.0;
                if(discrete) expt = discrete_state ? b.period.max_expt : 0.0;
                else expt = b.period.max_expt * axis_state;
                ctx.set_period_expt(b.period.modulator_index, b.id, expt);
            }
            break;
        case bind::AMPLITUDE_MUL:
            {
                float mul = 0.0;
                if(discrete)
                {
                    mul = discrete_state ?
                        b.amplitude.max_mul : b.amplitude.min_mul;
                }
                else mul = lerp(
                    b.amplitude.min_mul,
                    b.amplitude.max_mul,
                    axis_state_normalized
                );
                ctx.set_amplitude_mul(b.amplitude.modulator_index, b.id, mul);
            }
            break;
        }
    }
}

bind& bindings::create_new_bind()
{
    bind new_bind;
    new_bind.id = id_counter++;
    binds.push_back(new_bind);
    return binds.back();
}

bind& bindings::get_bind(unsigned i) { return binds[i]; }
const bind& bindings::get_bind(unsigned i) const { return binds[i]; }

void bindings::erase_bind(unsigned i)
{
    binds.erase(binds.begin() + i);
}

size_t bindings::bind_count() const
{
    return binds.size();
}

void bindings::save(json& j)
{
    // TODO: Implement
}

void bindings::load(const json& j)
{
    clear();
    // TODO: Implement
}

void bindings::clear()
{
    name = "New binding";
    device_type = "None";
    device_name = "Unnamed",
    id_counter = 0;
    binds.clear();
}
