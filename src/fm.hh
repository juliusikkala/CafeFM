#ifndef CAFEFM_FM_HH
#define CAFEFM_FM_HH
#include "func.hh"
#include "instrument.hh"
#include <cmath>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

enum oscillator_type
{
    OSC_SINE = 0,
    OSC_SQUARE,
    OSC_TRIANGLE,
    OSC_SAW
};

class basic_oscillator
{
public:
    struct context
    {
        context();

        void period_changed();

        int64_t time_offset, phase_offset;
        int64_t t, x;
    };

    basic_oscillator(double period = 1.0, double amplitude = 1.0);
    ~basic_oscillator();

    void set_amplitude(double amplitude, int64_t denom=65536);
    void set_amplitude(int64_t amp_num, int64_t amp_denom);
    double get_amplitude() const;
    void get_amplitude(int64_t& amp_num, int64_t& amp_denom) const;
    void set_period_fract(uint64_t period_num, uint64_t period_denom);
    void set_period(double period, uint64_t denom=65536);
    double get_period() const;
    void set_frequency(double freq, uint64_t samplerate);

protected:
    int64_t amp_num, amp_denom;
    uint64_t period_num, period_denom;
};

template<oscillator_type Type>
class oscillator: public basic_oscillator
{
public:
    oscillator() {}

    oscillator(const basic_oscillator& other)
    : basic_oscillator(other) {}

    oscillator& operator=(const basic_oscillator& other)
    {
        basic_oscillator::operator=(other);
        return *this;
    }

    inline int64_t func(
        context& ctx,
        int64_t t,
        uint64_t period_num,
        uint64_t period_denom,
        int64_t phase_shift = 0
    ) const {
        ctx.t = t;
        ctx.x = period_num * (t + ctx.time_offset)
            / period_denom + ctx.phase_offset;
        int64_t x = ctx.x + phase_shift;

        int64_t u;
        if constexpr(Type == OSC_SINE) u = i32sin(x);
        else if constexpr(Type == OSC_SQUARE) u = i32square(x);
        else if constexpr(Type == OSC_TRIANGLE) u = i32triangle(x);
        else if constexpr(Type == OSC_SAW) u = i32saw(x);
        return amp_num*u/amp_denom;
    }
};

class dynamic_oscillator: public basic_oscillator
{
public:
    dynamic_oscillator(
        oscillator_type type = OSC_SINE,
        double period = 1.0,
        double amplitude = 1.0
    );

    template<oscillator_type Type>
    dynamic_oscillator(const oscillator<Type>& osc)
    : basic_oscillator(osc), type(Type) {}

    void set_type(oscillator_type type);
    oscillator_type get_type() const;

    int64_t func(
        context& ctx,
        int64_t t,
        uint64_t period_num,
        uint64_t period_denom,
        int64_t phase_shift = 0
    ) const;

protected:
    oscillator_type type;
};

template<
    oscillator_type Type,
    oscillator_type... Modulators
> class modulator_stack: public oscillator<Type>
{
public:
    using context = std::pair<
        basic_oscillator::context,
        typename modulator_stack< Modulators...>::context
    >;

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
        uint64_t period_num,
        uint64_t period_denom
    ){
        if(i == 0)
        {
            oscillator<Type>::set_period_fract(period_num, period_denom);
        }
        else modulators.set_period_fract(i-1, period_num, period_denom);
    }

    void set_period(unsigned i, double period, uint64_t denom=65536)
    {
        if(i == 0)
        {
            oscillator<Type>::set_period(period, denom);
        }
        else modulators.set_period(i-1, period, denom);
    }

    void set_frequency(unsigned i, double freq, int64_t samplerate)
    {
        if(i == 0)
        {
            oscillator<Type>::set_frequency(freq, samplerate);
        }
        else modulators.set_frequency(i-1, freq, samplerate);
    }

    void period_changed(context& ctx)
    {
        ctx.first.period_changed();
        modulators.period_changed(ctx.second);
    }

    void import_oscillators(
        const std::vector<dynamic_oscillator>& oscillators,
        unsigned i = 0
    ){
        if(i >= oscillators.size()) return;
        oscillator<Type>::operator=(oscillators[i]);
        modulators.import_oscillators(oscillators, i+1);
    }

    void export_oscillators(std::vector<dynamic_oscillator>& oscillators) const
    {
        oscillators.push_back(*this);
        modulators.export_oscillators(oscillators);
    }

    inline int64_t calc_sample(
        context& ctx,
        int64_t x,
        uint64_t period_num,
        uint64_t period_denom
    ) const {
        period_num *= oscillator<Type>::period_num;
        period_denom *= oscillator<Type>::period_denom;
        normalize_fract(period_num, period_denom);
        return oscillator<Type>::func(
            ctx.first,
            x,
            period_num,
            period_denom,
            modulators.calc_sample(ctx.second, x, period_num, period_denom)
        );
    }

protected:
    modulator_stack<Modulators...> modulators;
};

template<oscillator_type Type>
class modulator_stack<Type>: public oscillator<Type>
{
public:
    using context = basic_oscillator::context;

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
        uint64_t period_num,
        uint64_t period_denom
    ){
        oscillator<Type>::set_period_fract(period_num, period_denom);
    }

    void set_period(unsigned i, double period, uint64_t denom=65536)
    {
        oscillator<Type>::set_period(period, denom);
    }

    void set_frequency(unsigned i, double freq, uint64_t samplerate)
    {
        oscillator<Type>::set_frequency(freq, samplerate);
    }

    void period_changed(context& ctx)
    {
        ctx.period_changed();
    }

    void import_oscillators(
        const std::vector<dynamic_oscillator>& oscillators,
        unsigned i = 0
    ){
        if(i >= oscillators.size()) return;
        oscillator<Type>::operator=(oscillators[i]);
    }

    void export_oscillators(std::vector<dynamic_oscillator>& oscillators) const
    {
        oscillators.push_back(*this);
    }

    inline int64_t calc_sample(
        context& ctx,
        int64_t x,
        uint64_t period_num,
        uint64_t period_denom
    ) const {
        period_num *= oscillator<Type>::period_num;
        period_denom *= oscillator<Type>::period_denom;
        normalize_fract(period_num, period_denom);
        return oscillator<Type>::func(ctx, x, period_num, period_denom);
    }
};

class constant_modulator
{
public:
    using context = std::monostate;
    void set_amplitude(...) {}
    void set_period_fract(...) {}
    void set_period(...) {}
    void set_frequency(...) {}
    void period_changed(...) {}
    void import_oscillators(...) {}
    void export_oscillators(...) const {}
};

template<
    oscillator_type Type,
    oscillator_type... Modulators
> class carrier_stack: public oscillator<Type>
{
public:
    using modulator_type = modulator_stack<Modulators...>;

    using context = std::pair<
        basic_oscillator::context,
        typename modulator_type::context
    >;

    carrier_stack(modulator_type* modulators): modulators(modulators) {}

    void period_changed(context& ctx)
    {
        ctx.first.period_changed();
        modulators->period_changed(ctx.second);
    }

    inline int64_t calc_sample(context& ctx, int64_t x)
    {
        return oscillator<Type>::func(
            ctx.first, x,
            oscillator<Type>::period_num,
            oscillator<Type>::period_denom,
            modulators->calc_sample(
                ctx.second, x,
                oscillator<Type>::period_num,
                oscillator<Type>::period_denom
            )
        );
    }
protected:
    modulator_type* modulators;
};

template<oscillator_type Type>
class carrier_stack<Type>: public oscillator<Type>
{
public:
    using modulator_type = constant_modulator;
    using context = basic_oscillator::context;

    carrier_stack(modulator_type* modulators) {}

    void period_changed(context& ctx)
    {
        ctx.period_changed();
    }

    inline int64_t calc_sample(context& ctx, int64_t x)
    {
        return oscillator<Type>::func(
            ctx, x,
            oscillator<Type>::period_num,
            oscillator<Type>::period_denom,
            0
        );
    }
};

class basic_fm_synth: public instrument
{
public:
    using instrument::instrument;

    virtual oscillator_type get_carrier_type() const = 0;

    virtual void import_modulators(
        const std::vector<dynamic_oscillator>& oscillators
    ) = 0;

    virtual void export_modulators(
        std::vector<dynamic_oscillator>& oscillators
    ) const = 0;
};

template<oscillator_type Carrier, oscillator_type... Modulators>
class fm_synth: public basic_fm_synth
{
public:
    fm_synth(uint64_t samplerate)
    : basic_fm_synth(samplerate), t(0)
    {
        carriers.resize(1, {&modulator});
        contexts.resize(1);
    }

    using carrier_type = carrier_stack<Carrier, Modulators...>;
    using modulator_type = typename carrier_type::modulator_type;

    void set_modulator(const modulator_type& s)
    {
        modulator = s;
        for(voice_id i = 0; i < carriers.size(); ++i)
            carriers[i].period_changed(contexts[i]);
    }

    const modulator_type& get_modulator() const {return modulator;}

    oscillator_type get_carrier_type() const override
    {
        return Carrier;
    }

    void import_modulators(
        const std::vector<dynamic_oscillator>& oscillators
    ) override
    {
        modulator.import_oscillators(oscillators, 0);
    }

    void export_modulators(
        std::vector<dynamic_oscillator>& oscillators
    ) const override
    {
        modulator.export_oscillators(oscillators);
    }

    void synthesize(int32_t* samples, unsigned sample_count) override
    {
        for(unsigned i = 0; i < sample_count; ++i, ++t)
        {
            for(voice_id j = 0; j < carriers.size(); ++j)
            {
                int64_t volume_num, volume_denom;
                get_voice_volume(j, volume_num, volume_denom);
                if(volume_num == 0) continue;

                carriers[j].set_amplitude(volume_num, volume_denom);
                samples[i] += carriers[j].calc_sample(contexts[j], t);
            }
            step_voices();
        }
    }

protected:
    void refresh_voice(voice_id id)
    {
        carriers[id].set_frequency(get_frequency(id), get_samplerate());
        carriers[id].period_changed(contexts[id]);
    }

    void handle_polyphony(unsigned n) override
    {
        if(n == 0) n = 1;
        carriers.resize(n, carriers[0]);
        contexts.resize(n);
    }

private:
    int64_t t;
    modulator_type modulator;
    std::vector<carrier_type> carriers;
    std::vector<typename carrier_type::context> contexts;
};

basic_fm_synth* create_fm_synth(
    uint64_t samplerate,
    const std::vector<oscillator_type>& oscillators
);

basic_fm_synth* create_fm_synth(
    uint64_t samplerate,
    oscillator_type carrier,
    const std::vector<dynamic_oscillator>& modulators
);

#endif
