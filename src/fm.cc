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
#include "fm.hh"
#include "helpers.hh"
#include <stdexcept>
#include <algorithm>
#define PERIOD_MUL 65536

static const char* const mode_strings[] = {
    "FREQUENCY", "PHASE"
};

static const char* const osc_strings[] = {
    "SINE", "SQUARE", "TRIANGLE", "SAW", "NOISE"
};

oscillator::state::state()
:  t(0), output(0) {}

oscillator::oscillator(
    func type,
    int64_t period_num,
    int64_t period_denom,
    double amplitude,
    double phase_constant
):  type(type), amp_num(1), amp_denom(1), period_num(1), period_denom(1),
    period_fine(0)
{
    set_period(period_num, period_denom);
    set_amplitude(amplitude);
    set_phase_constant(phase_constant);
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
        o.period_fine != period_fine ||
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

void oscillator::set_period(uint64_t period_num, uint64_t period_denom)
{
    this->period_num = period_num * PERIOD_MUL + period_fine;
    this->period_denom = period_denom * PERIOD_MUL;
    this->period_denom |= !period_denom;
}

void oscillator::get_period(uint64_t& period_num, uint64_t& period_denom) const
{
    period_num = (this->period_num - period_fine) / PERIOD_MUL;
    period_denom = this->period_denom / PERIOD_MUL;
}

void oscillator::set_period_fine(double fine)
{
    period_num -= period_fine;
    period_fine = std::max((int64_t)(fine * PERIOD_MUL), -(int64_t)period_num);
    period_num += period_fine;
}

double oscillator::get_period_fine() const
{
    return period_fine / (double)PERIOD_MUL;
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
    case NOISE:
        u = i32noise(t);
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
    uint64_t period_denom,
    uint64_t phase_offset
) const
{
    s.t += period_num/period_denom;
    s.output = value(s.t + phase_offset);
}

fm_synth::fm_synth()
: mode(FREQUENCY), oscillators{{}}, carriers{0} {}

bool fm_synth::index_compatible(const fm_synth& other) const
{
    if(
        other.carriers != carriers ||
        other.oscillators.size() != oscillators.size()
    ) return false;
    
    for(unsigned i = 0; i < oscillators.size(); ++i)
    {
        if(other.oscillators[i].modulators != oscillators[i].modulators)
            return false;
    }
    return true;
}

void fm_synth::set_modulation_mode(modulation_mode mode)
{
    this->mode = mode;
}

fm_synth::modulation_mode fm_synth::get_modulation_mode() const
{
    return mode;
}

std::vector<unsigned>& fm_synth::get_carriers()
{
    return carriers;
}

const std::vector<unsigned>& fm_synth::get_carriers() const
{
    return carriers;
}

unsigned fm_synth::get_oscillator_count() const
{
    return oscillators.size();
}

oscillator& fm_synth::get_oscillator(unsigned i) { return oscillators[i]; }
const oscillator& fm_synth::get_oscillator(unsigned i) const
{
    return oscillators[i];
}

void fm_synth::erase_oscillator(unsigned i)
{
    erase_index(i);
}

unsigned fm_synth::add_oscillator(const oscillator& o)
{
    oscillators.push_back(o);
    oscillators.back().modulators.clear();
    return oscillators.size()-1;
}

void fm_synth::finish_changes()
{
    erase_invalid_indices();
    reference_vec ref = determine_references();
    erase_orphans(ref);

    // Sort modulators
    std::map<int, int> index_map;
    std::vector<bool> handled_oscillators(oscillators.size(), false);
    std::vector<oscillator> new_oscillators;
    for(unsigned handled = 0; handled < oscillators.size(); ++handled)
    {
        int add_index = 0;
        int min_max_ref = oscillators.size();
        for(unsigned i = 0; i < oscillators.size(); ++i)
        {
            if(handled_oscillators[i]) continue;
            bool all_handled = true;
            int max_ref = -1;
            for(unsigned j = 0; j < ref[i].size(); ++j)
            {
                if(ref[i][j] == -1) continue;
                if(!handled_oscillators[ref[i][j]])
                {
                    all_handled = false;
                    break;
                }
                else if(index_map[ref[i][j]] > max_ref)
                    max_ref = index_map[ref[i][j]];
            }
            if(!all_handled) continue;
            if(max_ref < min_max_ref)
            {
                add_index = i;
                min_max_ref = max_ref;
            }
        }
        index_map[add_index] = new_oscillators.size();
        handled_oscillators[add_index] = true;
        new_oscillators.push_back(oscillators[add_index]);
    }

    oscillators = new_oscillators;

    // Apply new indices from index map
    for(unsigned& m: carriers) m = index_map[m];
    for(oscillator& o: oscillators)
        for(unsigned& m: o.modulators) m = index_map[m];

    sort_oscillators();
    update_period_lookup();
}

void fm_synth::update_period_lookup()
{
    period_lookup.resize(oscillators.size());
    std::fill(
        period_lookup.begin(),
        period_lookup.end(),
        std::make_pair(1lu, 1lu)
    );

    for(unsigned i = 0; i < oscillators.size(); ++i)
    {
        oscillator& o = oscillators[i];
        auto [period_num, period_denom] = period_lookup[i];
        period_num *= o.period_num;
        period_denom *= o.period_denom;
        normalize_fract(period_num, period_denom);
        period_lookup[i] = std::make_pair(period_num, period_denom);

        for(unsigned& m: o.modulators)
        {
            auto [num, denom] = period_lookup[m];
            num *= period_num;
            denom *= period_denom;
            normalize_fract(num, denom);
            period_lookup[m] = std::make_pair(num, denom);
        }
    }
}

fm_synth::layout fm_synth::generate_layout()
{
    reference_vec ref = determine_references();

    layout l;
    std::map<int /* parent */, int /* layer */> layer_map;
    layer_map[-1] = -1;

    // Since the parent should always be before in the array, a single pass
    // like this is enough.
    for(unsigned i = 0; i < oscillators.size(); ++i)
    {
        int parent = ref[i].front();
        unsigned layer = layer_map[parent] + 1;
        layer_map[i] = layer;

        if(layer >= l.layers.size())
            l.layers.resize(layer+1);

        // Find group with the same parent or create it.
        bool found = false;
        for(unsigned j = 0; j < l.layers[layer].size(); ++j)
        {
            layout::group& g = l.layers[layer][j];
            if(g.parent == parent)
            {
                g.oscillators.push_back(i);
                found = true;
                break;
            }
        }
        if(!found) l.layers[layer].push_back({parent, false, 1, {i}});
    }

    // Now, the array has all oscillator groups, but is unaligned and missing
    // +-buttons.

    // Set initial layer partition
    if(l.layers.size() > 0)
        l.layers[0][0].partition = l.layers[0][0].oscillators.size();

    // Since the first layer can't be unaligned, we don't have to care about it.
    for(unsigned i = 1; i < l.layers.size(); ++i)
    {
        layout::layer& prev_layer = l.layers[i-1];
        layout::layer& cur_layer = l.layers[i];
        layout::layer new_layer;

        // For every modulator _or_ filler on previous layer 
        for(unsigned j = 0; j < prev_layer.size(); ++j)
        {
            // Valid modulators go here
            if(prev_layer[j].oscillators.size())
            {
                for(unsigned k = 0; k < prev_layer[j].oscillators.size(); ++k)
                {
                    bool found = false;
                    int parent = prev_layer[j].oscillators[k];
                    // Find matching child group on current layer.
                    for(unsigned l = 0; l < cur_layer.size(); ++l)
                    {
                        if(cur_layer[l].parent == parent)
                        {
                            cur_layer[l].partition =
                                cur_layer[l].oscillators.size() *
                                prev_layer[j].partition;
                            new_layer.push_back(cur_layer[l]);
                            found = true;
                            break;
                        }
                    }
                    // Add +-button if there was no child group,
                    if(!found)
                    {
                        new_layer.push_back({
                            parent, false, prev_layer[j].partition, {}
                        });
                    }
                }
            }
            else // Fillers or +-buttons go here. They are followed by filler.
                new_layer.push_back({
                    prev_layer[j].parent, true, prev_layer[j].partition, {}
                });
        }
        cur_layer = new_layer;
    }

    // Add last layer of +-buttons and fillers.
    if(l.layers.size() > 0)
    {
        layout::layer& prev_layer = l.layers.back();
        layout::layer new_layer;
        // For every modulator _or_ filler on previous layer 
        for(unsigned j = 0; j < prev_layer.size(); ++j)
        {
            // Valid modulators go here
            if(prev_layer[j].oscillators.size())
            {
                for(unsigned k = 0; k < prev_layer[j].oscillators.size(); ++k)
                {
                    int parent = prev_layer[j].oscillators[k];
                    new_layer.push_back({
                        parent, false, prev_layer[j].partition, {}
                    });
                }
            }
            else new_layer.push_back({
                prev_layer[j].parent, true, prev_layer[j].partition, {}
            });
        }
        l.layers.push_back(new_layer);
    }
    // No modulators, so add lone +-button referencing carrier.
    else l.layers.push_back({{-1, false, 1, {}}});

    return l;
}

fm_synth::state fm_synth::start(
    double frequency, double volume, uint64_t samplerate, int64_t denom
) const
{
    state s;
    set_volume(s, volume*denom, denom);
    s.states.resize(oscillators.size());
    reset(s);
    return s;
}

void fm_synth::reset(state& s) const
{
    for(unsigned i = 0; i < oscillators.size(); ++i)
        oscillators[i].reset(s.states[i]);
}

int64_t fm_synth::step_frequency(state& s) const
{
    for(unsigned i = oscillators.size(); i > 0; --i)
    {
        const oscillator& o = oscillators[i-1];
        int64_t x = 1u<<31;
        for(unsigned m: o.modulators) x += s.states[m].output;

        uint64_t period_num = period_lookup[i-1].first;
        uint64_t period_denom = period_lookup[i-1].second;
        period_num *= s.period_num;
        period_denom *= s.period_denom;
        normalize_fract(period_num, period_denom);
        period_num *= x;
        period_denom <<= 31;
        o.update(s.states[i-1], period_num, period_denom);
    }

    int64_t x = 0;
    for(unsigned m: carriers) x += s.states[m].output;
    return s.amp_num*x/s.amp_denom;
}

int64_t fm_synth::step_phase(state& s) const
{
    for(unsigned i = oscillators.size(); i > 0; --i)
    {
        const oscillator& o = oscillators[i-1];
        int64_t x = 0;
        for(unsigned m: o.modulators) x += s.states[m].output;

        uint64_t period_num = period_lookup[i-1].first;
        uint64_t period_denom = period_lookup[i-1].second;
        period_num *= s.period_num;
        period_denom *= s.period_denom;
        o.update(s.states[i-1], period_num, period_denom, x);
    }

    int64_t x = 0;
    for(unsigned m: carriers) x += s.states[m].output;
    return s.amp_num*x/s.amp_denom;
}

void fm_synth::set_frequency(
    state& s, double frequency, uint64_t samplerate
) const
{
    s.period_num = round(frequency*4294967296.0/samplerate);
    s.period_denom = 1;
}

void fm_synth::set_volume(
    state& s, int64_t volume_num, int64_t volume_denom
) const
{
    s.amp_num = volume_num;
    s.amp_denom = volume_denom;
    s.amp_denom |= !s.amp_denom;
}

json fm_synth::serialize() const
{
    json j;
    j["mode"] = mode_strings[(unsigned)mode];
    j["carriers"] = carriers;
    j["oscillators"] = json::array();

    for(const oscillator& o: oscillators)
    {
        json m = {
            {"type", osc_strings[(unsigned)o.type]},
            {"amp_num", o.amp_num},
            {"amp_denom", o.amp_denom},
            {"period_num", (o.period_num-o.period_fine)/PERIOD_MUL},
            {"period_denom", o.period_denom/PERIOD_MUL},
            {"period_fine", o.period_fine/(double)PERIOD_MUL},
            {"phase_constant", o.phase_constant},
            {"modulators", o.modulators}
        };

        j["oscillators"].push_back(m);
    }

    return j;
}

bool fm_synth::deserialize(const json& j)
{
    oscillators.clear();
    carriers.clear();

    try
    {
        std::string mode_str = j.at("mode").get<std::string>();
        int mode_i = find_string_arg(mode_str.c_str(), mode_strings,
            sizeof(mode_strings)/sizeof(*mode_strings));
        if(mode_i < 0) return false;
        mode = (modulation_mode)mode_i;

        j.at("carriers").get_to(carriers);

        for(auto& m: j.at("oscillators"))
        {
            oscillator o;

            std::string type_str = m.at("type").get<std::string>();
            int type_i = find_string_arg(type_str.c_str(), osc_strings,
                sizeof(osc_strings)/sizeof(*osc_strings));
            if(type_i < 0) return false;
            o.type = (oscillator::func)type_i;
        
            double fine;
            m.at("amp_num").get_to(o.amp_num);
            m.at("amp_denom").get_to(o.amp_denom);
            m.at("period_num").get_to(o.period_num);
            m.at("period_denom").get_to(o.period_denom);
            m.at("period_fine").get_to(fine);
            m.at("phase_constant").get_to(o.phase_constant);
            m.at("modulators").get_to(o.modulators);

            o.period_fine = fine * PERIOD_MUL;
            o.period_num = o.period_num * PERIOD_MUL + o.period_fine;
            o.period_denom *= PERIOD_MUL;

            oscillators.push_back(o);
        }
    }
    catch(...)
    {
        oscillators.clear();
        carriers.clear();
        return false;
    }

    finish_changes();

    return true;
}

void fm_synth::erase_invalid_indices()
{
    for(unsigned j = 0; j < carriers.size(); ++j)
    {
        if(carriers[j] >= oscillators.size())
        {
            carriers.erase(carriers.begin() + j);
            j--;
        }
    }

    for(oscillator& o: oscillators)
    {
        for(unsigned j = 0; j < o.modulators.size(); ++j)
        {
            if(o.modulators[j] >= oscillators.size())
            {
                o.modulators.erase(o.modulators.begin() + j);
                j--;
            }
        }
    }
}

fm_synth::reference_vec fm_synth::determine_references()
{
    reference_vec references(oscillators.size());
    for(unsigned m: carriers) references[m].push_back(-1);
    for(unsigned i = 0; i < oscillators.size(); ++i)
    {
        for(unsigned m: oscillators[i].modulators)
            references[m].push_back(i);
    }
    return references;
}

void fm_synth::erase_orphans(reference_vec& ref)
{
    bool deleted = false;
    do
    {
        deleted = false;
        for(unsigned i = 0; i < ref.size(); ++i)
        {
            if(ref[i].size() == 0)
            {
                deleted = true;
                erase_index(i, &ref);
                i--;
            }
        }
    }
    while(deleted);
}

void fm_synth::erase_index(unsigned i, reference_vec* ref)
{
    // Remove the modulator itself
    oscillators.erase(oscillators.begin() + i);

    // Fix indices
    for(unsigned j = 0; j < carriers.size(); ++j)
    {
        if(carriers[j] == i)
        {
            carriers.erase(carriers.begin() + j);
            j--;
        }
        else if(carriers[j] > i) carriers[j]--;
    }

    for(oscillator& o: oscillators)
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

    // Update references if applicable
    if(ref)
    {
        ref->erase(ref->begin()+i);
        for(std::vector<int>& r: *ref)
        {
            for(unsigned j = 0; j < r.size(); ++j)
            {
                if(r[j] == (int)i)
                {
                    r.erase(r.begin() + j);
                    j--;
                }
                else if(r[j] > (int)i) r[j]--;
            }
        }
    }
}

void fm_synth::sort_oscillators()
{
    std::sort(carriers.begin(), carriers.end());
    for(oscillator& o: oscillators)
        std::sort(o.modulators.begin(), o.modulators.end());
}

fm_instrument::fm_instrument(uint64_t samplerate)
: instrument(samplerate)
{}

void fm_instrument::set_synth(const fm_synth& s)
{
    bool compatible = synth.index_compatible(s);
    // TODO: This might be unsafe, when the synth is compatible. That's because
    // synthesize() might be called simultaneously while this is modified, and
    // the vectors' internal pointers could possibly change in this call.
    // It would be better to manually copy these. (If the synth isn't
    // compatible, the audio output would be paused by the callee and
    // synthesize() couldn't be running.)
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
    // This is done by duplication to avoid testing mode in inner loops.
#define generate_samples(step_func) \
    for(unsigned i = 0; i < sample_count; ++i) \
    { \
        int64_t sum = 0; \
        for(voice_id j = 0; j < states.size(); ++j) \
        { \
            step_voice(j); \
            int64_t volume_num = 0, volume_denom; \
            get_voice_volume(j, volume_num, volume_denom); \
            if(volume_num == 0) continue; \
            synth.set_volume(states[j], volume_num, volume_denom); \
            sum += synth. step_func (states[j]); \
        } \
        samples[i] = std::clamp( \
            sum, (int64_t)INT32_MIN, (int64_t)INT32_MAX \
        ); \
    }

    switch(synth.get_modulation_mode())
    {
    case fm_synth::FREQUENCY:
        generate_samples(step_frequency)
        break;
    case fm_synth::PHASE:
        generate_samples(step_phase)
        break;
    }
#undef generate_samples
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

