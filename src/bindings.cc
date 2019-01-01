#include "bindings.hh"
#include "controller/controller.hh"
#include "helpers.hh"
#include <stdexcept>
#include <cstring>
#include <algorithm>

static const char* const control_strings[] = {
    "UNBOUND", "BUTTON_PRESS", "AXIS_1D_CONTINUOUS", "AXIS_1D_THRESHOLD"
};

static const char* const action_strings[] = {
    "KEY", "FREQUENCY_EXPT", "VOLUME_MUL", "PERIOD_EXPT", "AMPLITUDE_MUL",
    "ENVELOPE_ADJUST"
};

bind::bind(enum action a)
:   id(0), wait_assign(false), control(UNBOUND), toggle(false),
    cumulative(false), action(a)
{
    switch(action)
    {
    case KEY:
        key_semitone = 0;
        break;
    case FREQUENCY_EXPT:
        frequency.max_expt = 12;
        break;
    case VOLUME_MUL:
        volume.max_mul = 0.5;
        break;
    case PERIOD_EXPT:
        period.modulator_index = 0;
        period.max_expt = 12;
        break;
    case AMPLITUDE_MUL:
        amplitude.modulator_index = 0;
        amplitude.max_mul = 0.5;
        break;
    case ENVELOPE_ADJUST:
        envelope.which = 3;
        envelope.max_mul = 10.0;
        break;
    }
}

json bind::serialize() const
{
    json j;

    j["control"]["type"] = control_strings[(unsigned)control];
    switch(control)
    {
    case BUTTON_PRESS:
        j["control"]["index"] = button.index;
        j["control"]["active_state"] = button.active_state;
        break;
    case AXIS_1D_CONTINUOUS:
    case AXIS_1D_THRESHOLD:
        j["control"]["index"] = axis_1d.index;
        j["control"]["invert"] = axis_1d.invert;
        j["control"]["threshold"] = axis_1d.threshold;
        break;
    default:
        break;
    }

    if(control != AXIS_1D_CONTINUOUS)
        j["control"]["toggle"] = toggle;
    j["control"]["cumulative"] = cumulative;

    j["action"]["type"] = action_strings[(unsigned)action];

    switch(action)
    {
    case KEY:
        j["action"]["semitone"] = key_semitone;
        break;
    case FREQUENCY_EXPT:
        j["action"]["frequency"]["max_expt"] = frequency.max_expt;
        break;
    case VOLUME_MUL:
        j["action"]["volume"]["max_mul"] = volume.max_mul;
        break;
    case PERIOD_EXPT:
        j["action"]["period"]["modulator_index"] = period.modulator_index;
        j["action"]["period"]["max_expt"] = period.max_expt;
        break;
    case AMPLITUDE_MUL:
        j["action"]["amplitude"]["modulator_index"] = amplitude.modulator_index;
        j["action"]["amplitude"]["max_mul"] = amplitude.max_mul;
        break;
    case ENVELOPE_ADJUST:
        j["action"]["envelope"]["which"] = envelope.which;
        j["action"]["envelope"]["max_mul"] = envelope.max_mul;
        break;
    }

    return j;
}

bool bind::deserialize(const json& j)
{
    try
    {
        std::string control_str = j.at("control").at("type").get<std::string>();
        int control_i = find_string_arg(
            control_str.c_str(),
            control_strings,
            sizeof(control_strings)/sizeof(*control_strings)
        );
        if(control_i < 0) return false;
        control = (enum control)control_i;

        switch(control)
        {
        case BUTTON_PRESS:
            j.at("control").at("index").get_to(button.index);
            j.at("control").at("active_state").get_to(button.active_state);
            break;
        case AXIS_1D_CONTINUOUS:
        case AXIS_1D_THRESHOLD:
            j.at("control").at("index").get_to(axis_1d.index);
            j.at("control").at("invert").get_to(axis_1d.invert);
            j.at("control").at("threshold").get_to(axis_1d.threshold);
            break;
        default:
            break;
        }

        toggle = j.at("control").value("toggle", false);
        cumulative = j.at("control").value("cumulative", false);

        std::string action_str = j.at("action").at("type").get<std::string>();
        int action_i = find_string_arg(
            action_str.c_str(),
            action_strings,
            sizeof(action_strings)/sizeof(*action_strings)
        );
        if(action_i < 0) return false;
        action = (enum action)action_i;

        switch(action)
        {
        case KEY:
            j.at("action").at("semitone").get_to(key_semitone);
            break;
        case FREQUENCY_EXPT:
            j.at("action").at("frequency")
                .at("max_expt").get_to(frequency.max_expt);
            break;
        case VOLUME_MUL:
            j.at("action").at("volume").at("max_mul").get_to(volume.max_mul);
            break;
        case PERIOD_EXPT:
            j.at("action").at("period")
                .at("modulator_index").get_to(period.modulator_index);
            j.at("action").at("period").at("max_expt").get_to(period.max_expt);
            break;
        case AMPLITUDE_MUL:
            j.at("action").at("amplitude")
                .at("modulator_index").get_to(amplitude.modulator_index);
            j.at("action").at("amplitude")
                .at("max_mul").get_to(amplitude.max_mul);
            break;
        case ENVELOPE_ADJUST:
            j.at("action").at("envelope").at("which").get_to(envelope.which);
            j.at("action").at("envelope")
                .at("max_mul").get_to(envelope.max_mul);
            break;
        }
    }
    catch(...)
    {
        return false;
    }

    return true;
}

bool bind::triggered(
    int axis_1d_index,
    int axis_2d_index,
    int button_index
) const {
    if(control == UNBOUND) return false;

    switch(control)
    {
    case BUTTON_PRESS:
        return button_index >= 0 && button_index == button.index;
    case AXIS_1D_CONTINUOUS:
    case AXIS_1D_THRESHOLD:
        return axis_1d_index >= 0 && axis_1d_index == axis_1d.index;
    default:
        throw std::runtime_error(
            "Unknown bind control " + std::to_string(control)
        );
    }
}

double bind::input_value(const controller* c, bool* is_signed) const
{
    if(control == UNBOUND) return 0.0;

    double value = 0.0;

    switch(control)
    {
    case BUTTON_PRESS:
        value = c->get_button_state(button.index) == button.active_state
            ? 1.0 : 0.0;
        break;
    case AXIS_1D_CONTINUOUS:
    case AXIS_1D_THRESHOLD:
        {
            ::axis_1d ax = c->get_axis_1d_state(axis_1d.index);
            value = ax.value;
            if(is_signed != nullptr) *is_signed = ax.is_signed;

            if(axis_1d.invert)
            {
                if(ax.is_signed || !ax.is_limited) value = -value;
                else value = 1.0 - value;
            }
        }
        break;
    default:
        break;
    }

    return value;
}

double bind::get_value(const control_state& state, const controller* c) const
{
    double v = input_value(c);
    if(control == AXIS_1D_THRESHOLD) v = v > axis_1d.threshold ? 1.0 : 0.0;

    if(toggle)
    {
        int prev_state = state.get_toggle_state(id);
        // Mealy state machine. States:
        // 0 - disabled, input inactive
        // 1 - enabled, input active
        // 2 - enabled, input inactive
        // 3 - disabled, input active
        if(prev_state == 0) v = 0.0;
        else if(prev_state == 1) v = 1.0;
        else if(prev_state == 2) v = 1.0;
        else if(prev_state == 3) v = 0.0;
    }

    return v;
}

bool bind::update_value(
    control_state& state,
    const controller* c,
    double& v
) const {
    v = input_value(c);
    if(control == AXIS_1D_THRESHOLD) v = v > axis_1d.threshold ? 1.0 : 0.0;

    if(toggle)
    {
        int prev_state = state.get_toggle_state(id);
        bool d = v > 0.5;
        // Mealy state machine. States:
        // 0 - disabled, input inactive
        // 1 - enabled, input active
        // 2 - enabled, input inactive
        // 3 - disabled, input active
        if(prev_state == 0)
        {
            if(d)
            {
                v = 1.0;
                state.set_toggle_state(id, 1);
            }
            else v = 0.0;
        }
        else if(prev_state == 1)
        {
            if(!d) state.set_toggle_state(id, 2);
            return false;
        }
        else if(prev_state == 2)
        {
            if(d)
            {
                v = 0.0;
                state.set_toggle_state(id, 3);
            }
            else v = 1.0;
        }
        else if(prev_state == 3)
        {
            if(!d) state.set_toggle_state(id, 0);
            return false;
        }
    }
    return true;
}

double bind::normalize(const controller* c, double v) const
{
    if(control == AXIS_1D_CONTINUOUS)
    {
        ::axis_1d ax = c->get_axis_1d_state(axis_1d.index);
        if(ax.is_signed || !ax.is_limited)
        {
            v = (v + 1.0)*0.5;
            if(v < 0.0) v = 0.0;
            else if(v > 1.0) v = 1.0;
        }
    }
    return v;
}

bindings::bindings() { clear(); }

void bindings::set_write_lock(bool lock)
{
    write_lock = lock;
}

bool bindings::is_write_locked() const
{
    return write_lock;
}

void bindings::set_name(const std::string& name) { this->name = name; }
std::string bindings::get_name() const { return name; }

void bindings::set_path(const fs::path& path)
{
    this->path = path;
}

fs::path bindings::get_path() const
{
    return path;
}

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
            if(b.button.index >= button_count) index_match = false;
            break;
        case bind::AXIS_1D_CONTINUOUS:
        case bind::AXIS_1D_THRESHOLD:
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
        else return 2;
    }
    else if(index_match) return 3;
    else return 4;
}

void bindings::cumulative_update(control_state& state)
{
    for(const bind& b: binds)
    {
        if(b.cumulative)
            handle_action(state, b, state.get_cumulation(b.id));
    }
}

void bindings::act(
    control_state& state,
    controller* c,
    int axis_1d_index,
    int axis_2d_index,
    int button_index
){
    for(const bind& b: binds)
    {
        // Skip if this bind wasn't triggered
        if(!b.triggered(axis_1d_index, axis_2d_index, button_index)) continue;

        double value = 0.0;
        if(!b.update_value(state, c, value)) continue;

        // Handle cumulative actions.
        if(b.cumulative)
        {
            state.set_cumulation_speed(b.id, value);
            continue;
        }

        if(
            b.action == bind::VOLUME_MUL ||
            b.action == bind::AMPLITUDE_MUL
        ) value = b.normalize(c, value);

        // Perform actual action.
        handle_action(state, b, value);
    }
}

bind& bindings::create_new_bind(enum bind::action action)
{
    bind new_bind(action);
    new_bind.id = id_counter++;
    binds.push_back(new_bind);
    return binds.back();
}

bind& bindings::get_bind(unsigned i) { return binds[i]; }
const bind& bindings::get_bind(unsigned i) const { return binds[i]; }
const std::vector<bind>& bindings::get_binds() const { return binds; }

void bindings::move_bind(
    unsigned i,
    int movement,
    control_state& state,
    bool same_action
){
    if(i >= binds.size()) return;

    bind& b = binds[i];

    if(movement == -2) erase_bind(i, state);
    else if(movement == 1)
    {
        unsigned new_index = i;
        if(!same_action)
        {
            if(new_index > 0) new_index--;
        }
        else
        {
            // Find first action above with same action.
            while(new_index > 0)
            {
                new_index--;
                if(binds[new_index].action == b.action) break;
            }
        }
        auto start = binds.rbegin() + binds.size() - i - 1;
        auto end = binds.rbegin() + binds.size() - new_index;
        std::rotate(start, start+1, end);
    }
    else if(movement == -1)
    {
        unsigned new_index = i;

        if(!same_action)
        {
            if(new_index < binds.size()-1) new_index++;
        }
        else
        {
            // Find first action above with same action.
            while(new_index < binds.size()-1)
            {
                new_index++;
                if(binds[new_index].action == b.action) break;
            }
        }

        auto start = binds.begin() + i;
        auto end = binds.begin() + new_index + 1;
        std::rotate(start, start+1, end);
    }
}

void bindings::erase_bind(unsigned i, control_state& state)
{
    auto it = binds.begin() + i;
    state.erase_action(it->id);
    binds.erase(it);
}

size_t bindings::bind_count() const
{
    return binds.size();
}

json bindings::serialize() const
{
    json j;
    j["name"] = name;
    j["locked"] = write_lock;
    j["controller_type"] = device_type;
    j["device_name"] = device_name;
    j["binds"] = json::array();
    for(const auto& b: binds)
    {
        if(b.control != bind::UNBOUND) j["binds"].push_back(b.serialize());
    }
    return j;
}

bool bindings::deserialize(const json& j)
{
    clear();

    try
    {
        j.at("name").get_to(name);
        j.at("locked").get_to(write_lock);
        j.at("controller_type").get_to(device_type);
        j.at("device_name").get_to(device_name);

        for(auto& b: j.at("binds"))
        {
            bind new_bind;
            new_bind.id = id_counter++;
            if(!new_bind.deserialize(b)) return false;
            binds.push_back(new_bind);
        }
    }
    catch(...)
    {
        return false;
    }

    return true;
}

void bindings::clear()
{
    name = "New binding";
    path.clear();
    device_type = "None";
    device_name = "Unnamed",
    id_counter = 0;
    write_lock = false;
    binds.clear();
}

void bindings::handle_action(control_state& state, const bind& b, double value)
{
    switch(b.action)
    {
    case bind::KEY:
        if(value)
        {
            if(!state.is_active_key(b.id))
                state.press_key(b.id, b.key_semitone);
        }
        else state.release_key(b.id);
        break;
    case bind::FREQUENCY_EXPT:
        state.set_frequency_expt(b.id, b.frequency.max_expt * value);
        break;
    case bind::VOLUME_MUL:
        state.set_volume_mul(
            b.id,
            b.cumulative ?
                pow(b.volume.max_mul, value):
                lerp(1.0, b.volume.max_mul, value)
        );
        break;
    case bind::PERIOD_EXPT:
        state.set_period_expt(
            b.period.modulator_index,
            b.id,
            b.period.max_expt * value
        );
        break;
    case bind::AMPLITUDE_MUL:
        state.set_amplitude_mul(
            b.amplitude.modulator_index,
            b.id,
            b.cumulative ? 
                pow(b.amplitude.max_mul, value):
                lerp(1.0, b.amplitude.max_mul, value)
        );
        break;
    case bind::ENVELOPE_ADJUST:
        state.set_envelope_adjust(
            b.envelope.which,
            b.id,
            b.cumulative ? 
                pow(b.envelope.max_mul, value):
                lerp(1.0, b.envelope.max_mul, value)
        );
        break;
    }
}
