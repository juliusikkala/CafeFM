#include "action.hh"
#include <stdexcept>

action_context::action_context() { }

void action_context::queue(const action& a)
{
    switch(a.type)
    {
    case action::PRESS_KEY:
        press_queue.emplace_back(a.id, a.semitone);
        break;
    case action::RELEASE_KEY:
        release_queue.emplace_back(a.id);
        break;
    case action::SET_FREQUENCY_EXPT:
        freq_expt[a.id] = a.freq_expt;
        break;
    case action::SET_VOLUME_MUL:
        volume_mul[a.id] = a.volume_mul;
        break;
    case action::SET_PERIOD_EXPT:
        osc[a.period.modulator_index].period_expt[a.id] = a.period.expt;
        break;
    case action::SET_AMPLITUDE_MUL:
        osc[a.amplitude.modulator_index].amplitude_mul[a.id] = a.amplitude.mul;
        break;
    default:
        throw std::runtime_error(
            "Unknown action type " + std::to_string(a.type)
        );
    }
}

void action_context::reset()
{
    dst_modulators.clear();
    press_queue.clear();
    release_queue.clear();
    pressed_keys.clear();
    freq_expt.clear();
    volume_mul.clear();

    for(unsigned i = 0; i < sizeof(osc)/sizeof(*osc); ++i)
    {
        osc[i].period_expt.clear();
        osc[i].amplitude_mul.clear();
    }
}

double action_context::total_freq_mul() const
{
    double expt = 0.0;
    for(auto& pair: freq_expt) expt += pair.second;
    return 440.0*pow(2.0, expt/12.0);
}

double action_context::total_volume_mul() const
{
    double mul = 1.0;
    for(auto& pair: volume_mul) mul *= pair.second;
    return mul;
}

double action_context::total_period_mul(unsigned i) const
{
    double expt = 0.0;
    for(auto& pair: osc[i].period_expt) expt += pair.second;
    return pow(2.0, expt/12.0);
}

double action_context::total_amp_mul(unsigned i) const
{
    double mul = 1.0;
    for(auto& pair: osc[i].amplitude_mul) mul *= pair.second;
    return mul;
}

void action_context::apply(
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

    for(action::source_id id: release_queue)
    {
        for(auto& pair: pressed_keys)
            if(pair.second == id) synth.release_voice(pair.first);
    }

    release_queue.clear();
}

