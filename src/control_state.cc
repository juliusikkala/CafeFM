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
#include "control_state.hh"
#include "bindings.hh"
#include <stdexcept>

namespace
{
    template<typename T>
    void reset_controller(T& m, control_state::controller_id cid)
    {
        for(auto it = m.begin(); it != m.end();)
        {
            auto old_it = it++;
            if((uint32_t)old_it->first == cid)
                m.erase(old_it);
        }
    }
}

control_state::full_id control_state::create_id(
    controller_id cid,
    action_id aid
){
    return ((uint64_t)cid) | (((uint64_t)aid)<<32);
}

void control_state::split_id(full_id id, controller_id& cid, action_id& aid)
{
    cid = id&0xFFFFFFFF;
    aid = id>>32;
}

control_state::control_state() { }

void control_state::set_threshold_state(
    controller_id cid,
    action_id aid,
    int active
){
    full_id id = create_id(cid, aid);
    threshold_state[id] = active;
}

int control_state::get_threshold_state(controller_id cid, action_id aid) const
{
    full_id id = create_id(cid, aid);
    auto it = threshold_state.find(id);
    if(it == threshold_state.end()) return 0;
    return it->second;
}

void control_state::set_toggle_state(
    controller_id cid,
    action_id aid,
    int state
){
    full_id id = create_id(cid, aid);
    toggle_state[id] = state;
}

int control_state::get_toggle_state(controller_id cid, action_id aid) const
{
    full_id id = create_id(cid, aid);
    auto it = toggle_state.find(id);
    if(it == toggle_state.end()) return 0;
    return it->second;
}

void control_state::set_cumulation_speed(
    controller_id cid,
    action_id aid,
    double speed
){
    full_id id = create_id(cid, aid);
    auto it = cumulative_state.find(id);
    if(it == cumulative_state.end())
        cumulative_state[id] = std::make_pair(0.0, speed);
    else it->second.second = speed;
}

void control_state::clear_cumulation(controller_id cid, action_id aid)
{
    full_id id = create_id(cid, aid);
    cumulative_state.erase(id);
}

double control_state::get_cumulation(controller_id cid, action_id aid)
{
    full_id id = create_id(cid, aid);
    auto it = cumulative_state.find(id);
    if(it == cumulative_state.end()) return 0;
    return it->second.first;
}

void control_state::set_stacking(controller_id cid, action_id aid, int s)
{
    full_id id = create_id(cid, aid);
    stacking[id] = s;
}

int control_state::get_stacking(controller_id cid, action_id aid) const
{
    full_id id = create_id(cid, aid);
    auto it = stacking.find(id);
    if(it == stacking.end()) return 0;
    return it->second;
}

void control_state::erase_action(controller_id cid, action_id aid)
{
    full_id id = create_id(cid, aid);
    for(unsigned i = 0; i < press_queue.size(); ++i)
    {
        if(press_queue[i].id == id)
        {
            press_queue.erase(press_queue.begin() + i);
            i--;
        }
    }

    release_key(cid, aid);

    threshold_state.erase(id);
    toggle_state.erase(id);
    stacking.erase(id);
    cumulative_state.erase(id);

    freq_expt.erase(id);
    volume_mul.erase(id);

    for(auto& o: osc)
    {
        o.period_fine.erase(id);
        o.amplitude_mul.erase(id);
    }

    for(unsigned i = 0; i < sizeof(env)/sizeof(*env); ++i)
        env[i].mul.erase(id);
}

void control_state::press_key(
    controller_id cid,
    action_id aid,
    int semitone,
    double volume
){
    full_id id = create_id(cid, aid);
    press_queue.push_back({id, semitone, volume});
}

void control_state::set_key_volume(
    controller_id cid,
    action_id aid,
    double volume
){
    full_id id = create_id(cid, aid);
    for(auto& pair: pressed_keys)
        if(pair.second.id == id)
            pair.second.volume = volume;
}

void control_state::release_key(controller_id cid, action_id aid)
{
    full_id id = create_id(cid, aid);
    release_queue.emplace_back(id);
}

bool control_state::is_active_key(controller_id cid, action_id aid) const
{
    full_id id = create_id(cid, aid);
    for(const auto& pair: pressed_keys)
        if(pair.second.id == id) return true;
    return false;
}

void control_state::set_frequency_expt(
    controller_id cid,
    action_id aid,
    double freq_expt
){
    full_id id = create_id(cid, aid);
    this->freq_expt[id] = freq_expt;
}

bool control_state::get_frequency_expt(
    controller_id cid,
    action_id aid,
    double& freq_expt
) const
{
    full_id id = create_id(cid, aid);
    auto it = this->freq_expt.find(id);
    if(it == this->freq_expt.end()) return false;
    freq_expt = it->second;
    return true;
}

void control_state::set_volume_mul(
    controller_id cid,
    action_id aid,
    double volume_mul
){
    full_id id = create_id(cid, aid);
    this->volume_mul[id] = volume_mul;
}

bool control_state::get_volume_mul(
    controller_id cid,
    action_id aid,
    double& volume_mul
) const
{
    full_id id = create_id(cid, aid);
    auto it = this->volume_mul.find(id);
    if(it == this->volume_mul.end()) return false;
    volume_mul = it->second;
    return true;
}

void control_state::set_period_fine(
    unsigned modulator_index,
    controller_id cid,
    action_id aid,
    double period_fine
){
    full_id id = create_id(cid, aid);
    if(modulator_index >= osc.size())
        osc.resize(modulator_index+1);
    osc[modulator_index].period_fine[id] = period_fine;
}

bool control_state::get_period_fine(
    unsigned modulator_index,
    controller_id cid,
    action_id aid,
    double& period_fine
) const {
    full_id id = create_id(cid, aid);
    if(modulator_index >= osc.size()) return false;
    auto it = osc[modulator_index].period_fine.find(id);
    if(it == osc[modulator_index].period_fine.end()) return false;
    period_fine = it->second;
    return true;
}

void control_state::set_amplitude_mul(
    unsigned modulator_index,
    controller_id cid,
    action_id aid,
    double amplitude_mul
) {
    full_id id = create_id(cid, aid);
    if(modulator_index >= osc.size())
        osc.resize(modulator_index+1);
    osc[modulator_index].amplitude_mul[id] = amplitude_mul;
}

bool control_state::get_amplitude_mul(
    unsigned modulator_index,
    controller_id cid,
    action_id aid,
    double& amplitude_mul
) const {
    full_id id = create_id(cid, aid);
    if(modulator_index >= osc.size()) return false;
    auto it = osc[modulator_index].amplitude_mul.find(id);
    if(it == osc[modulator_index].amplitude_mul.end()) return false;
    amplitude_mul = it->second;
    return true;
}

void control_state::set_envelope_adjust(
    unsigned which,
    controller_id cid,
    action_id aid,
    double mul
){
    full_id id = create_id(cid, aid);
    env[which].mul[id] = mul;
}

bool control_state::get_envelope_adjust(
    unsigned which,
    controller_id cid,
    action_id aid,
    double& mul
) const
{
    full_id id = create_id(cid, aid);
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
    threshold_state.clear();
    toggle_state.clear();
    stacking.clear();
    cumulative_state.clear();
    freq_expt.clear();
    volume_mul.clear();

    for(auto& o: osc)
    {
        o.period_fine.clear();
        o.amplitude_mul.clear();
    }

    for(unsigned i = 0; i < sizeof(env)/sizeof(*env); ++i)
    {
        env[i].mul.clear();
    }
}

void control_state::reset(controller_id cid)
{
    for(unsigned i = 0; i < press_queue.size(); ++i)
    {
        if((uint32_t)press_queue[i].id == cid)
        {
            press_queue.erase(press_queue.begin() + i);
            --i;
        }
    }

    for(unsigned i = 0; i < release_queue.size(); ++i)
    {
        if((uint32_t)release_queue[i] == cid)
        {
            release_queue.erase(release_queue.begin() + i);
            --i;
        }
    }

    for(auto it = pressed_keys.begin(); it != pressed_keys.end();)
    {
        auto old_it = it++;
        if((uint32_t)old_it->second.id == cid)
            pressed_keys.erase(old_it);
    }

    reset_controller(threshold_state, cid);
    reset_controller(toggle_state, cid);
    reset_controller(stacking, cid);
    reset_controller(cumulative_state, cid);
    reset_controller(freq_expt, cid);
    reset_controller(volume_mul, cid);

    for(auto& o: osc)
    {
        reset_controller(o.period_fine, cid);
        reset_controller(o.amplitude_mul, cid);
    }

    for(unsigned i = 0; i < sizeof(env)/sizeof(*env); ++i)
    {
        reset_controller(env[i].mul, cid);
    }
}

void control_state::update(controller_id cid, bindings& b, double dt)
{
    bool changed = false;
    for(auto& pair: cumulative_state)
    {
        double change = pair.second.second * dt;
        if(change != 0)
        {
            pair.second.first += change;
            changed = true;
        }
    }

    if(changed)
        b.cumulative_update(cid, *this);
}

double control_state::total_freq_mul(double base_freq) const
{
    double expt = 0.0;
    for(auto& pair: freq_expt) expt += pair.second;
    return base_freq*pow(2.0, expt/12.0);
}

double control_state::total_volume_mul() const
{
    double mul = 1.0;
    for(auto& pair: volume_mul) mul *= pair.second;
    return mul;
}

double control_state::total_period_fine(unsigned i) const
{
    double fine = 0.0;
    if(i >= osc.size()) return fine;

    for(auto& pair: osc[i].period_fine) fine += pair.second;
    return fine;
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
        i < osc.size() && i < dst.get_oscillator_count();
        ++i
    ){
        oscillator& mod = dst.get_oscillator(i);
        int64_t amp_num, amp_denom;
        double fine = mod.get_period_fine();
        mod.get_amplitude(amp_num, amp_denom);

        amp_num *= total_amp_mul(i);

        mod.set_amplitude(amp_num, amp_denom);
        mod.set_period_fine(fine + total_period_fine(i));
    }

    ins.set_tuning(total_freq_mul(ins_state.tuning_frequency));

    envelope adsr = ins_state.adsr;
    adsr.attack_length *= total_envelope_adjust(0);
    adsr.decay_length *= total_envelope_adjust(1);
    adsr.sustain_volume_num *= total_envelope_adjust(2);
    adsr.release_length *= total_envelope_adjust(3);

    ins.set_envelope(adsr);
    ins.set_volume(src_volume*total_volume_mul());
    dst.update_period_lookup();
    dst.limit_total_carrier_amplitude();
    ins.set_synth(dst);

    for(key_data& p: press_queue)
        pressed_keys[ins.press_voice(p.semitone)] = p;

    press_queue.clear();

    for(auto it = pressed_keys.begin(); it != pressed_keys.end();)
    {
        auto old_it = it++;
        ins.set_voice_volume(old_it->first, old_it->second.volume);
        for(full_id id: release_queue)
        {
            if(old_it->second.id == id)
            {
                ins.release_voice(old_it->first);
                pressed_keys.erase(old_it);
                break;
            }
        }
    }

    release_queue.clear();
}

