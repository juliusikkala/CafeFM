#ifndef CAFEFM_FM_HH
#define CAFEFM_FM_HH
#include "func.hh"
#include <cmath>
#include <cstddef>

enum oscillator_type
{
    OSC_SINE = 0,
    OSC_SQUARE,
    OSC_TRIANGLE,
    OSC_SAW
};

template<oscillator_type Type>
class oscillator
{
public:
    oscillator()
    : amp_num(1), amp_denom(1), period_num(1)
    {
    }

    void set_amplitude(double amplitude, int64_t denom=65536)
    {
        amp_num = amplitude * denom;
        amp_denom = denom;
    }

    void set_period(double period, int64_t denom=1l<<32)
    {
        period_num = denom/period;
        period_denom = denom;
    }

    void set_frequency(double freq, int64_t samplerate)
    {
        period_num = round(freq*4294967296.0/samplerate);
        period_denom = 1;
    }

protected:
    inline int64_t func(int64_t x)
    {
        int64_t u;
        if constexpr(Type == OSC_SINE) u = i32sin(x);
        else if constexpr(Type == OSC_SQUARE) u = i32square(x);
        else if constexpr(Type == OSC_TRIANGLE) u = i32triangle(x);
        else if constexpr(Type == OSC_SAW) u = i32saw(x);
        return amp_num*u/amp_denom;
    }

    int64_t amp_num, amp_denom;
    int64_t period_num, period_denom;
};

template<oscillator_type Type, oscillator_type... Modulators>
class static_oscillator_stack: protected oscillator<Type>
{
public:
    void set_amplitude(unsigned i, double amplitude, int64_t denom=65536)
    {
        if(i == 0) oscillator<Type>::set_amplitude(amplitude, denom);
        else modulators.set_amplitude(i-1, amplitude, denom);
    }

    void set_period(unsigned i, double period, int64_t denom=1l<<32)
    {
        if(i == 0) oscillator<Type>::set_period(period, denom);
        else modulators.set_period(i-1, period, denom);
    }

    void set_frequency(unsigned i, double freq, int64_t samplerate)
    {
        if(i == 0) oscillator<Type>::set_frequency(freq, samplerate);
        else modulators.set_frequency(i-1, freq, samplerate);
    }

    inline int64_t calc_sample(int64_t x)
    {
        x = oscillator<Type>::period_num*x/oscillator<Type>::period_denom;
        int64_t phase_shift = modulators.calc_sample(x);
        return oscillator<Type>::func(x+phase_shift);
    }

protected:
    static_oscillator_stack<Modulators...> modulators;
};

template<oscillator_type Type>
class static_oscillator_stack<Type>: protected oscillator<Type>
{
public:
    void set_amplitude(unsigned i, double amplitude, int64_t denom=65536)
    {
        oscillator<Type>::set_amplitude(amplitude, denom);
    }

    void set_period(unsigned i, double period, int64_t denom=1l<<32)
    {
        oscillator<Type>::set_period(period, denom);
    }

    void set_frequency(unsigned i, double freq, int64_t samplerate)
    {
        oscillator<Type>::set_frequency(freq, samplerate);
    }

    inline int64_t calc_sample(int64_t x)
    {
        x = oscillator<Type>::period_num*x/oscillator<Type>::period_denom;
        return oscillator<Type>::func(x);
    }
};

template<oscillator_type... Modulators>
class static_fm_synth: public static_oscillator_stack<Modulators...>
{
public:
    static_fm_synth()
    : t(0)
    {
    }

    using static_oscillator_stack<Modulators...>::set_amplitude;
    using static_oscillator_stack<Modulators...>::set_period;
    using static_oscillator_stack<Modulators...>::set_frequency;
    using static_oscillator_stack<Modulators...>::calc_sample;

    void set_frequency(double freq, int64_t samplerate)
    {
        set_frequency(0, freq, samplerate);
    }

    void synthesize(int32_t* samples, unsigned sample_count)
    {
        for(unsigned i = 0; i < sample_count; ++i, ++t)
            samples[i] = calc_sample(t);
    }
private:
    int64_t t;
};

#endif
