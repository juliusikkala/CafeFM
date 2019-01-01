#include "control_state.hh"
#include <stdexcept>

control_state::control_state() { }

void control_state::set_toggle_state(action_id id, int state)
{
    toggle_state[id] = state;
}

int control_state::get_toggle_state(action_id id) const
{
    auto it = toggle_state.find(id);
    if(it == toggle_state.end()) return 0;
    return it->second;
}

void control_state::erase_action(action_id id)
{
    for(unsigned i = 0; i < press_queue.size(); ++i)
    {
        if(press_queue[i].first == id)
        {
            press_queue.erase(press_queue.begin() + i);
            i--;
        }
    }

    release_key(id);

    toggle_state.erase(id);

    freq_expt.erase(id);
    volume_mul.erase(id);

    for(unsigned i = 0; i < sizeof(osc)/sizeof(*osc); ++i)
    {
        osc[i].period_expt.erase(id);
        osc[i].amplitude_mul.erase(id);
    }
}

void control_state::press_key(action_id id, int semitone)
{
    press_queue.emplace_back(id, semitone);
}

void control_state::release_key(action_id id)
{
    release_queue.emplace_back(id);
}

bool control_state::is_active_key(action_id id) const
{
    for(const auto& pair: pressed_keys)
        if(pair.second == id) return true;
    return false;
}

void control_state::set_frequency_expt(action_id id, double freq_expt)
{
    this->freq_expt[id] = freq_expt;
}

bool control_state::get_frequency_expt(action_id id, double& freq_expt) const
{
    auto it = this->freq_expt.find(id);
    if(it == this->freq_expt.end()) return false;
    freq_expt = it->second;
    return true;
}

void control_state::set_volume_mul(action_id id, double volume_mul)
{
    this->volume_mul[id] = volume_mul;
}

bool control_state::get_volume_mul(action_id id, double& volume_mul) const
{
    auto it = this->volume_mul.find(id);
    if(it == this->volume_mul.end()) return false;
    volume_mul = it->second;
    return true;
}

void control_state::set_period_expt(
    unsigned modulator_index, action_id id, double period_expt
){
    osc[modulator_index].period_expt[id] = period_expt;
}

bool control_state::get_period_expt(
    unsigned modulator_index, action_id id, double& period_expt
) const {
    auto it = osc[modulator_index].period_expt.find(id);
    if(it == osc[modulator_index].period_expt.end()) return false;
    period_expt = it->second;
    return true;
}

void control_state::set_amplitude_mul(
    unsigned modulator_index, action_id id, double amplitude_mul
) {
    osc[modulator_index].amplitude_mul[id] = amplitude_mul;
}

bool control_state::get_amplitude_mul(
    unsigned modulator_index, action_id id, double& amplitude_mul
) const {
    auto it = osc[modulator_index].amplitude_mul.find(id);
    if(it == osc[modulator_index].amplitude_mul.end()) return false;
    amplitude_mul = it->second;
    return true;
}

void control_state::reset()
{
    dst_modulators.clear();
    press_queue.clear();
    release_queue.clear();
    pressed_keys.clear();
    toggle_state.clear();
    freq_expt.clear();
    volume_mul.clear();

    for(unsigned i = 0; i < sizeof(osc)/sizeof(*osc); ++i)
    {
        osc[i].period_expt.clear();
        osc[i].amplitude_mul.clear();
    }
}

double control_state::total_freq_mul() const
{
    double expt = 0.0;
    for(auto& pair: freq_expt) expt += pair.second;
    return 440.0*pow(2.0, expt/12.0);
}

double control_state::total_volume_mul() const
{
    double mul = 1.0;
    for(auto& pair: volume_mul) mul *= pair.second;
    return mul;
}

double control_state::total_period_mul(unsigned i) const
{
    double expt = 0.0;
    for(auto& pair: osc[i].period_expt) expt += pair.second;
    return pow(2.0, expt/12.0);
}

double control_state::total_amp_mul(unsigned i) const
{
    double mul = 1.0;
    for(auto& pair: osc[i].amplitude_mul) mul *= pair.second;
    return mul;
}

void control_state::apply(
    basic_fm_synth& synth,
    double src_volume,
    const std::vector<dynamic_oscillator>& src_modulators
){
    dst_modulators = src_modulators;

    for(
        unsigned i = 0;
        i < sizeof(osc)/sizeof(*osc) && i < dst_modulators.size();
        ++i
    ){
        int64_t amp_num, amp_denom;
        uint64_t period_num, period_denom;
        dst_modulators[i].get_amplitude(amp_num, amp_denom);
        dst_modulators[i].get_period(period_num, period_denom);

        amp_num *= total_amp_mul(i);
        period_denom *= total_period_mul(i);

        dst_modulators[i].set_amplitude(amp_num, amp_denom);
        dst_modulators[i].set_period_fract(period_num, period_denom);
    }

    synth.set_tuning(total_freq_mul());
    synth.set_volume(src_volume*total_volume_mul());
    synth.import_modulators(dst_modulators);

    for(auto& pair: press_queue)
        pressed_keys[synth.press_voice(pair.second)] = pair.first;

    press_queue.clear();

    for(action_id id: release_queue)
    {
        for(auto& pair: pressed_keys)
            if(pair.second == id) synth.release_voice(pair.first);
    }

    release_queue.clear();
}

