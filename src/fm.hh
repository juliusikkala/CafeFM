#ifndef CAFEFM_FM_HH
#define CAFEFM_FM_HH
#include "func.hh"
#include "instrument.hh"
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
    : amp_num(1), amp_denom(1), period_num(1), period_denom(1), time_offset(0), phase_offset(0), t(0), x(0)
    {
    }

    void set_amplitude(double amplitude, int64_t denom=65536)
    {
        amp_num = amplitude * denom;
        amp_denom = denom;
        amp_denom |= !amp_denom;
    }

    void set_amplitude(int64_t num, int64_t denom)
    {
        amp_num = num;
        amp_denom = denom;
        amp_denom |= !amp_denom;
    }

    void set_period_fract(uint64_t period_denom, uint64_t period_num)
    {
        this->period_num = period_num;
        this->period_denom = period_denom;
        this->period_denom |= !period_denom;
        period_changed();
    }

    void set_period(double period, uint64_t denom=65536)
    {
        period_num = denom;
        period_denom = denom * period;
        period_denom |= !period_denom;
        period_changed();
    }

    void set_frequency(double freq, uint64_t samplerate)
    {
        period_num = round(freq*4294967296.0/samplerate);
        period_denom = 1;
        period_changed();
    }

    void period_changed()
    {
        time_offset = -t;
        // TODO: It's probably valid to &0xFFFFFFFF this, check if so. It would
        // be nice to avoid overflows in prolonged play.
        phase_offset = x;
    }

protected:
    inline int64_t func(
        int64_t t,
        uint64_t period_num,
        uint64_t period_denom,
        int64_t phase_shift = 0
    ){
        this->t = t;
        this->x = period_num * (t + time_offset) / period_denom + phase_offset;
        int64_t x = this->x + phase_shift;

        int64_t u;
        if constexpr(Type == OSC_SINE) u = i32sin(x);
        else if constexpr(Type == OSC_SQUARE) u = i32square(x);
        else if constexpr(Type == OSC_TRIANGLE) u = i32triangle(x);
        else if constexpr(Type == OSC_SAW) u = i32saw(x);
        return amp_num*u/amp_denom;
    }

    int64_t amp_num, amp_denom;
    uint64_t period_num, period_denom;
    int64_t time_offset, phase_offset;
    int64_t t, x;
};

template<oscillator_type Type, oscillator_type... Modulators>
class static_oscillator_stack: public oscillator<Type>
{
public:
    void set_amplitude(unsigned i, double amplitude, int64_t denom=65536)
    {
        if(i == 0) oscillator<Type>::set_amplitude(amplitude, denom);
        else modulators.set_amplitude(i-1, amplitude, denom);
    }

    void set_amplitude(unsigned i, int64_t num, int64_t denom)
    {
        if(i == 0) oscillator<Type>::set_amplitude(num, denom);
        else modulators.set_amplitude(i-1, num, denom);
    }

    void set_period_fract(
        unsigned i,
        uint64_t period_denom,
        uint64_t period_num
    ){
        if(i == 0)
        {
            oscillator<Type>::set_period_fract(period_denom, period_num);
            recursive_period_changed();
        }
        else modulators.set_period_fract(i-1, period_denom, period_num);
    }

    void set_period(unsigned i, double period, uint64_t denom=65536)
    {
        if(i == 0)
        {
            oscillator<Type>::set_period(period, denom);
            recursive_period_changed();
        }
        else modulators.set_period(i-1, period, denom);
    }

    void set_frequency(unsigned i, double freq, int64_t samplerate)
    {
        if(i == 0)
        {
            oscillator<Type>::set_frequency(freq, samplerate);
            recursive_period_changed();
        }
        else modulators.set_frequency(i-1, freq, samplerate);
    }

    inline int64_t calc_sample(
        int64_t x,
        uint64_t period_num,
        uint64_t period_denom
    ){
        period_num *= oscillator<Type>::period_num;
        period_denom *= oscillator<Type>::period_denom;
        normalize_fract(period_num, period_denom);
        return oscillator<Type>::func(
            x,
            period_num,
            period_denom,
            modulators.calc_sample(x, period_num, period_denom)
        );
    }

    using oscillator<Type>::period_changed;
    void recursive_period_changed()
    {
        modulators.period_changed();
        modulators.recursive_period_changed();
    }

protected:
    static_oscillator_stack<Modulators...> modulators;
};

template<oscillator_type Type>
class static_oscillator_stack<Type>: public oscillator<Type>
{
public:
    void set_amplitude(unsigned i, double amplitude, int64_t denom=65536)
    {
        oscillator<Type>::set_amplitude(amplitude, denom);
    }

    void set_amplitude(unsigned i, int64_t num, int64_t denom)
    {
        oscillator<Type>::set_amplitude(num, denom);
    }

    void set_period_fract(
        unsigned i,
        uint64_t period_denom,
        uint64_t period_num
    ){
        oscillator<Type>::set_period_fract(period_denom, period_num);
    }

    void set_period(unsigned i, double period, uint64_t denom=65536)
    {
        oscillator<Type>::set_period(period, denom);
    }

    void set_frequency(unsigned i, double freq, uint64_t samplerate)
    {
        oscillator<Type>::set_frequency(freq, samplerate);
    }

    inline int64_t calc_sample(
        int64_t x,
        uint64_t period_num,
        uint64_t period_denom
    ){
        period_num *= oscillator<Type>::period_num;
        period_denom *= oscillator<Type>::period_denom;
        normalize_fract(period_num, period_denom);
        return oscillator<Type>::func(x, period_num, period_denom);
    }

    void recursive_period_changed() {}
};

template<oscillator_type... Modulators>
class static_fm_synth: public instrument
{
public:
    static_fm_synth(uint64_t samplerate)
    : instrument(samplerate), t(0)
    {
        stacks.resize(1);
    }

    using stack = static_oscillator_stack<Modulators...>;

    void set_stack(const stack& s)
    {
        for(voice_id i = 0; i < stacks.size(); ++i)
        {
            stacks[i] = s;
            stacks[i].set_frequency(0, get_frequency(i), get_samplerate());
        }
    }

    const stack& get_stack() const {return stacks[0];}

    void synthesize(int32_t* samples, unsigned sample_count) override
    {
        for(unsigned i = 0; i < sample_count; ++i, ++t)
        {
            samples[i] = 0;
            for(voice_id j = 0; j < stacks.size(); ++j)
            {
                int64_t volume_num, volume_denom;
                get_voice_volume(j, volume_num, volume_denom);
                if(volume_num == 0) continue;

                stacks[j].set_amplitude(0, volume_num, volume_denom);
                samples[i] += stacks[j].calc_sample(t, 1, 1);
            }
            step_voices();
        }
    }

protected:
    void refresh_voice(voice_id id)
    {
        stacks[id].set_frequency(0, get_frequency(id), get_samplerate());
    }

    void handle_polyphony(unsigned n) override
    {
        if(n == 0) n = 1;
        stacks.resize(n, stacks[0]);
    }

private:
    int64_t t;
    std::vector<stack> stacks;
};

#endif
