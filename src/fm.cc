#include "fm.hh"

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
