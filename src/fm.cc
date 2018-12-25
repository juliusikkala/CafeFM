#include "fm.hh"
#include <stdexcept>

basic_oscillator::context::context()
: time_offset(0), phase_offset(0), t(0), x(0) {}

void basic_oscillator::context::period_changed()
{
    time_offset = -t;
    // TODO: It's probably valid to &0xFFFFFFFF this, check if so. It would
    // be nice to avoid overflows in prolonged play.
    phase_offset = x;
}

basic_oscillator::basic_oscillator()
: amp_num(1), amp_denom(1), period_num(1), period_denom(1) {}

basic_oscillator::~basic_oscillator() {}

void basic_oscillator::set_amplitude(double amplitude, int64_t denom)
{
    amp_num = amplitude * denom;
    amp_denom = denom;
    amp_denom |= !amp_denom;
}

void basic_oscillator::set_amplitude(int64_t num, int64_t denom)
{
    amp_num = num;
    amp_denom = denom;
    amp_denom |= !amp_denom;
}

void basic_oscillator::set_period_fract(
    uint64_t period_num,
    uint64_t period_denom
){
    this->period_num = period_num;
    this->period_denom = period_denom;
    this->period_denom |= !period_denom;
}

void basic_oscillator::set_period(double period, uint64_t denom)
{
    period_num = denom;
    period_denom = denom * period;
    period_denom |= !period_denom;
}

void basic_oscillator::set_frequency(double freq, uint64_t samplerate)
{
    period_num = round(freq*4294967296.0/samplerate);
    period_denom = 1;
}

// This is why the compilation is so extremely slow :D
template<unsigned depth, oscillator_type... Modulators>
instrument* fm_search_tree(
    uint64_t samplerate,
    const std::vector<oscillator_type>& oscillators,
    unsigned start_index = 0
){
    if constexpr(depth == 0)
        throw std::runtime_error("Search tree for oscillators ran out!");
    else {
        oscillator_type t = oscillators[start_index];
        if (oscillators.size()-1 == start_index)
        {
            switch(t)
            {
            case OSC_SINE:
                return new fm_synth<Modulators..., OSC_SINE>(samplerate);
            case OSC_SQUARE:
                return new fm_synth<Modulators..., OSC_SQUARE>(samplerate);
            case OSC_TRIANGLE:
                return new fm_synth<Modulators...,OSC_TRIANGLE>(samplerate);
            case OSC_SAW:
                return new fm_synth<Modulators..., OSC_SAW>(samplerate);
            default:
                throw std::runtime_error(
                    "Unknown oscillator type " + std::to_string(t)
                );
            }
        }
        else
        {
            switch(t)
            {
            case OSC_SINE:
                return fm_search_tree<depth-1, Modulators..., OSC_SINE>(
                    samplerate, oscillators, start_index+1
                );
            case OSC_SQUARE:
                return fm_search_tree<depth-1, Modulators..., OSC_SQUARE>(
                    samplerate, oscillators, start_index+1
                );
            case OSC_TRIANGLE:
                return fm_search_tree<depth-1, Modulators..., OSC_TRIANGLE>(
                    samplerate, oscillators, start_index+1
                );
            case OSC_SAW:
                return fm_search_tree<depth-1, Modulators..., OSC_SAW>(
                    samplerate, oscillators, start_index+1
                );
            default:
                throw std::runtime_error(
                    "Unknown oscillator type " + std::to_string(t)
                );
            }
        }
    }
}

instrument* create_fm_synth(
    uint64_t samplerate,
    const std::vector<oscillator_type>& oscillators
){
    if(oscillators.size() > 4)
        throw std::runtime_error(
            "Over 4 oscillators in FM synth not supported"
        );
    if(oscillators.size() == 0)
        return nullptr;

    // You can increase this from 4 to higher numbers at the expense of
    // exploding compile time and binary size
    return fm_search_tree<4>(samplerate, oscillators);
}
