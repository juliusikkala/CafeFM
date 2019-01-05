#include "control_state.hh"
#include "bindings.hh"
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

void control_state::set_cumulation_speed(action_id id, double speed)
{
    auto it = cumulative_state.find(id);
    if(it == cumulative_state.end())
        cumulative_state[id] = std::make_pair(0.0, speed);
    else it->second.second = speed;
}

void control_state::clear_cumulation(action_id id)
{
    cumulative_state.erase(id);
}

double control_state::get_cumulation(action_id id)
{
    auto it = cumulative_state.find(id);
    if(it == cumulative_state.end()) return 0;
    return it->second.first;
}

void control_state::set_stacking(action_id id, int s)
{
    stacking[id] = s;
}

int control_state::get_stacking(action_id id) const
{
    auto it = stacking.find(id);
    if(it == stacking.end()) return 0;
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
    stacking.erase(id);
    cumulative_state.erase(id);

    freq_expt.erase(id);
    volume_mul.erase(id);

    for(auto& o: osc)
    {
        o.period_expt.erase(id);
        o.amplitude_mul.erase(id);
    }

    for(unsigned i = 0; i < sizeof(env)/sizeof(*env); ++i)
        env[i].mul.erase(id);
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
    if(modulator_index >= osc.size())
        osc.resize(modulator_index+1);
    osc[modulator_index].period_expt[id] = period_expt;
}

bool control_state::get_period_expt(
    unsigned modulator_index, action_id id, double& period_expt
) const {
    if(modulator_index >= osc.size()) return false;
    auto it = osc[modulator_index].period_expt.find(id);
    if(it == osc[modulator_index].period_expt.end()) return false;
    period_expt = it->second;
    return true;
}

void control_state::set_amplitude_mul(
    unsigned modulator_index, action_id id, double amplitude_mul
) {
    if(modulator_index >= osc.size())
        osc.resize(modulator_index+1);
    osc[modulator_index].amplitude_mul[id] = amplitude_mul;
}

bool control_state::get_amplitude_mul(
    unsigned modulator_index, action_id id, double& amplitude_mul
) const {
    if(modulator_index >= osc.size()) return false;
    auto it = osc[modulator_index].amplitude_mul.find(id);
    if(it == osc[modulator_index].amplitude_mul.end()) return false;
    amplitude_mul = it->second;
    return true;
}

void control_state::set_envelope_adjust(
    unsigned which, action_id id, double mul
){
    env[which].mul[id] = mul;
}

bool control_state::get_envelope_adjust(
    unsigned which, action_id id, double& mul
) const
{
    auto it = env[which].mul.find(id);
    if(it == env[which].mul.end()) return false;
    mul = it->second;
    return true;
}

void control_state::reset()
{
    press_queue.clear();
    release_queue.clear();
    pressed_keys.clear();
    toggle_state.clear();
    stacking.clear();
    cumulative_state.clear();
    freq_expt.clear();
    volume_mul.clear();

    for(auto& o: osc)
    {
        o.period_expt.clear();
        o.amplitude_mul.clear();
    }

    for(unsigned i = 0; i < sizeof(env)/sizeof(*env); ++i)
    {
        env[i].mul.clear();
    }
}

void control_state::update(bindings& b, unsigned dt)
{
    bool changed = false;
    for(auto& pair: cumulative_state)
    {
        double change = pair.second.second * dt / 1000.0;
        if(change != 0)
        {
            pair.second.first += change;
            changed = true;
        }
    }

    if(changed)
        b.cumulative_update(*this);
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
    if(i >= osc.size()) return expt;

    for(auto& pair: osc[i].period_expt) expt += pair.second;
    return pow(2.0, expt/12.0);
}

double control_state::total_amp_mul(unsigned i) const
{
    double mul = 1.0;
    if(i >= osc.size()) return mul;

    for(auto& pair: osc[i].amplitude_mul) mul *= pair.second;
    return mul;
}

double control_state::total_envelope_adjust(unsigned which) const
{
    double mul = 1.0;
    for(auto& pair: env[which].mul) mul *= pair.second;
    return mul;
}

void control_state::apply(
    fm_instrument& ins,
    double src_volume,
    instrument_state& ins_state
){
    fm_synth dst = ins_state.synth;

    for(
        unsigned i = 0;
        i < osc.size() && i < dst.get_modulator_count();
        ++i
    ){
        oscillator& mod = dst.get_modulator(i);
        int64_t amp_num, amp_denom;
        uint64_t period_num, period_denom;
        mod.get_amplitude(amp_num, amp_denom);
        mod.get_period(period_num, period_denom);

        amp_num *= total_amp_mul(i);
        period_denom *= total_period_mul(i);

        mod.set_amplitude(amp_num, amp_denom);
        mod.set_period_fract(period_num, period_denom);
    }

    ins.set_tuning(total_freq_mul());

    envelope adsr = ins_state.adsr;
    adsr.attack_length *= total_envelope_adjust(0);
    adsr.decay_length *= total_envelope_adjust(1);
    adsr.sustain_volume_num *= total_envelope_adjust(2);
    adsr.release_length *= total_envelope_adjust(3);

    ins.set_envelope(adsr);
    ins.set_volume(src_volume*total_volume_mul());
    ins.set_synth(dst);

    for(auto& pair: press_queue)
        pressed_keys[ins.press_voice(pair.second)] = pair.first;

    press_queue.clear();

    for(action_id id: release_queue)
    {
        for(auto it = pressed_keys.begin(); it != pressed_keys.end();)
        {
            auto old_it = it++;
            if(old_it->second == id)
            {
                ins.release_voice(old_it->first);
                pressed_keys.erase(old_it);
            }
        }
    }

    release_queue.clear();
}

