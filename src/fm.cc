#include "fm.hh"
#include "helpers.hh"
#include <stdexcept>

static const char* const osc_strings[] = {
    "SINE", "SQUARE", "TRIANGLE", "SAW"
};

oscillator::state::state()
:  t(0), output(0) {}

oscillator::oscillator(
    func type,
    double period,
    double amplitude,
    double phase_constant
): type(type), amp_num(1), amp_denom(1), period_num(1), period_denom(1) {
    set_period(period);
    set_amplitude(amplitude);
    set_phase_constant(phase_constant);
}

oscillator::oscillator(
    func type,
    double frequency,
    double volume,
    uint64_t samplerate
): type(type), phase_constant(0)
{
    set_frequency(frequency, samplerate);
    set_amplitude(volume);
}

oscillator::~oscillator() {}

bool oscillator::operator!=(const oscillator& o) const
{
    return (
        o.type != type ||
        o.amp_num != amp_num ||
        o.amp_denom != amp_denom ||
        o.period_num != period_num ||
        o.period_denom != period_denom ||
        o.phase_constant != phase_constant ||
        o.modulators != modulators
    );
}

void oscillator::set_type(func type)
{
    this->type = type;
}

oscillator::func oscillator::get_type() const
{
    return type;
}

void oscillator::set_amplitude(double amplitude, int64_t denom)
{
    amp_num = amplitude * denom;
    amp_denom = denom;
    amp_denom |= !amp_denom;
}

void oscillator::set_amplitude(int64_t num, int64_t denom)
{
    amp_num = num;
    amp_denom = denom;
    amp_denom |= !amp_denom;
}

double oscillator::get_amplitude() const 
{
    return amp_num/(double)amp_denom;
}

void oscillator::get_amplitude(int64_t& amp_num, int64_t& amp_denom) const
{
    amp_num = this->amp_num;
    amp_denom = this->amp_denom;
}

void oscillator::set_period_fract(
    uint64_t period_num,
    uint64_t period_denom
){
    this->period_num = period_num;
    this->period_denom = period_denom;
    this->period_denom |= !period_denom;
}

void oscillator::set_period(double period, uint64_t denom)
{
    period_num = denom;
    period_denom = denom * period;
    period_denom |= !period_denom;
}

double oscillator::get_period() const
{
    return period_denom/(double)period_num;
}

void oscillator::get_period(
    uint64_t& period_num,
    uint64_t& period_denom
) const
{
    period_num = this->period_num;
    period_denom = this->period_denom;
}

void oscillator::set_frequency(double freq, uint64_t samplerate)
{
    period_num = round(freq*4294967296.0/samplerate);
    period_denom = 1;
}

void oscillator::set_phase_constant(int64_t offset)
{
    phase_constant = offset&0xFFFFFFFFlu;
}

void oscillator::set_phase_constant(double offset)
{
    phase_constant = ((int64_t)round(offset*4294967296.0))&0xFFFFFFFFlu;
}

int64_t oscillator::get_phase_constant() const
{
    return phase_constant;
}

double oscillator::get_phase_constant_double() const
{
    return phase_constant/4294967296.0;
}

std::vector<unsigned>& oscillator::get_modulators()
{
    return modulators;
}

const std::vector<unsigned>&  oscillator::get_modulators() const
{
    return modulators;
}

int64_t oscillator::value(int64_t t) const
{
    int64_t u = 0;
    switch(type)
    {
    case SINE:
        u = i32sin(t);
        break;
    case SQUARE:
        u = i32square(t);
        break;
    case TRIANGLE:
        u = i32triangle(t);
        break;
    case SAW:
        u = i32saw(t);
        break;
    }
    return amp_num*u/amp_denom;
}

void oscillator::reset(state& s) const
{
    s.t = phase_constant;
    s.output = value(s.t);
}

void oscillator::update(
    state& s,
    uint64_t period_num,
    uint64_t period_denom
) const
{
    period_num *= this->period_num;
    period_denom *= this->period_denom;
    s.t += period_num/period_denom;
    s.output = value(s.t);
}

fm_synth::fm_synth()
: carrier_type(oscillator::SINE) {}

bool fm_synth::index_compatible(const fm_synth& other) const
{
    if(
        other.carrier_modulators != carrier_modulators ||
        other.modulators.size() != modulators.size()
    ) return false;
    
    for(unsigned i = 0; i < modulators.size(); ++i)
    {
        if(other.modulators[i].modulators != modulators[i].modulators)
            return false;
    }
    return true;
}

void fm_synth::set_carrier_type(oscillator::func carrier_type)
{
    this->carrier_type = carrier_type;
}
oscillator::func fm_synth::get_carrier_type() const
{
    return carrier_type;
}

std::vector<unsigned>& fm_synth::get_carrier_modulators()
{
    return carrier_modulators;
}

const std::vector<unsigned>& fm_synth::get_carrier_modulators() const
{
    return carrier_modulators;
}

unsigned fm_synth::get_modulator_count() const
{
    return modulators.size();
}

oscillator& fm_synth::get_modulator(unsigned i) { return modulators[i]; }
const oscillator& fm_synth::get_modulator(unsigned i) const
{
    return modulators[i];
}

void fm_synth::erase_modulator(unsigned i)
{
    // Remove the modulators of this modulator
    std::vector<unsigned> m = modulators[i].modulators;
    std::sort(m.begin(), m.end());
    // Reverse order to avoid messing earlier indices
    for(auto it = m.rbegin(); it != m.rend(); ++it)
    {
        // This should never happen, but let's not take the chance.
        if(*it <= i) continue;
        erase_modulator(*it);
    }

    // Remove the modulator itself
    modulators.erase(modulators.begin() + i);

    // Fix indices
    for(unsigned j = 0; j < carrier_modulators.size(); ++j)
    {
        if(carrier_modulators[j] == i)
        {
            carrier_modulators.erase(carrier_modulators.begin() + j);
            j--;
        }
        else if(carrier_modulators[j] > i) carrier_modulators[j]--;
    }

    for(oscillator& o: modulators)
    {
        for(unsigned j = 0; j < o.modulators.size(); ++j)
        {
            if(o.modulators[j] == i)
            {
                o.modulators.erase(o.modulators.begin() + j);
                j--;
            }
            else if(o.modulators[j] > i) o.modulators[j]--;
        }
    }
}

unsigned fm_synth::add_modulator(const oscillator& o)
{
    modulators.push_back(o);
    modulators.back().modulators.clear();
    return modulators.size()-1;
}

fm_synth::state fm_synth::start(
    double frequency, double volume, uint64_t samplerate
) const
{
    state s;
    s.carrier = oscillator(carrier_type, frequency, volume, samplerate);
    s.states.resize(1 + modulators.size());
    reset(s);
    return s;
}

void fm_synth::reset(state& s) const
{
    s.carrier.reset(s.states[0]);
    for(unsigned i = 0; i < modulators.size(); ++i)
        modulators[i].reset(s.states[i+1]);
}

int64_t fm_synth::step(state& s) const
{
    for(unsigned i = modulators.size(); i > 0; --i)
    {
        const oscillator& o = modulators[i-1];
        int64_t x = 1u<<31;
        for(unsigned m: o.modulators) x += s.states[m+1].output;

        uint64_t period_num = s.carrier.period_num;
        uint64_t period_denom = s.carrier.period_denom;
        period_num = period_num * x;
        period_denom <<= 31;
        normalize_fract(period_num, period_denom);
        o.update(s.states[i], period_num, period_denom);
    }

    int64_t x = 1u<<31;
    for(unsigned m: carrier_modulators) x += s.states[m+1].output;

    s.carrier.update(s.states[0], x, 1l<<31);

    return s.states[0].output;
}

void fm_synth::set_frequency(
    state& s, double frequency, uint64_t samplerate
) const
{
    s.carrier.set_frequency(frequency, samplerate);
}

void fm_synth::set_volume(
    state& s, int64_t volume_num, int64_t volume_denom
) const
{
    s.carrier.set_amplitude(volume_num, volume_denom);
}

json fm_synth::serialize() const
{
    json j;
    j["carrier"] = {
        {"type", osc_strings[(unsigned)carrier_type]},
        {"modulators", carrier_modulators}
    };
    j["modulators"] = json::array();

    for(const oscillator& o: modulators)
    {
        json m = {
            {"type", osc_strings[(unsigned)o.type]},
            {"amp_num", o.amp_num},
            {"amp_denom", o.amp_denom},
            {"period_num", o.period_num},
            {"period_denom", o.period_denom},
            {"phase_constant", o.phase_constant},
            {"modulators", o.modulators}
        };

        j["modulators"].push_back(m);
    }

    return j;
}

bool fm_synth::deserialize(const json& j)
{
    modulators.clear();
    carrier_modulators.clear();

    try
    {
        std::string carrier_str = j.at("carrier").at("type").get<std::string>();
        int carrier_i = find_string_arg(carrier_str.c_str(), osc_strings,
            sizeof(osc_strings)/sizeof(*osc_strings));
        if(carrier_i < 0) return false;
        carrier_type = (oscillator::func)carrier_i;

        j.at("carrier").at("modulators").get_to(carrier_modulators);

        for(auto& m: j.at("modulators"))
        {
            oscillator o;

            std::string type_str = m.at("type").get<std::string>();
            int type_i = find_string_arg(type_str.c_str(), osc_strings,
                sizeof(osc_strings)/sizeof(*osc_strings));
            if(type_i < 0) return false;
            o.type = (oscillator::func)type_i;
        
            m.at("amp_num").get_to(o.amp_num);
            m.at("amp_denom").get_to(o.amp_denom);
            m.at("period_num").get_to(o.period_num);
            m.at("period_denom").get_to(o.period_denom);
            m.at("phase_constant").get_to(o.phase_constant);
            m.at("modulators").get_to(o.modulators);

            modulators.push_back(o);
        }
    }
    catch(...)
    {
        return false;
    }

    return true;
}

fm_instrument::fm_instrument(uint64_t samplerate)
: instrument(samplerate)
{}

void fm_instrument::set_synth(const fm_synth& s)
{
    bool compatible = synth.index_compatible(s);
    synth = s;
    if(compatible) return;

    for(voice_id j = 0; j < states.size(); ++j)
    {
        states[j] = synth.start();
        reset_voice(j);
    }
}

const fm_synth& fm_instrument::get_synth()
{
    return synth;
}

void fm_instrument::synthesize(int32_t* samples, unsigned sample_count) 
{
    // TODO: Consider making the inner loop outer for cache reasons.
    // Doing that requires changing step_voices to step_voice.
    for(unsigned i = 0; i < sample_count; ++i)
    {
        for(voice_id j = 0; j < states.size(); ++j)
        {
            int64_t volume_num, volume_denom;
            get_voice_volume(j, volume_num, volume_denom);
            if(volume_num == 0) continue;

            synth.set_volume(states[j], volume_num, volume_denom);
            samples[i] += synth.step(states[j]);
        }
        step_voices();
    }
}

void fm_instrument::refresh_voice(voice_id id)
{
    synth.set_frequency(states[id], get_frequency(id), get_samplerate());
}

void fm_instrument::reset_voice(voice_id id)
{
    synth.set_frequency(states[id], get_frequency(id), get_samplerate());
    synth.reset(states[id]);
}

void fm_instrument::handle_polyphony(unsigned n)
{
    if(n == 0) n = 1;
    states.resize(n, synth.start());
}

