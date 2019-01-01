#include "synth_state.hh"
#include "helpers.hh"

static const char* const osc_strings[] = {
    "SINE", "SQUARE", "TRIANGLE", "SAW"
};

synth_state::synth_state(uint64_t samplerate)
: name("Unnamed synth"), carrier(OSC_SINE), polyphony(6), write_lock(false)
{
    adsr.set_volume(1.0f, 0.5f);
    adsr.set_curve(0.07f, 0.2f, 0.05f, samplerate);
}

basic_fm_synth* synth_state::create_synth(uint64_t samplerate) const
{
    basic_fm_synth* res = create_fm_synth(
        samplerate, carrier, modulators
    );
    res->set_volume(1.0/polyphony);
    res->set_polyphony(polyphony);

    return res;
}

json synth_state::serialize(uint64_t samplerate) const
{
    json j;
    j["name"] = name;
    j["carrier"] = osc_strings[(unsigned)carrier];
    j["polyphony"] = polyphony;
    j["modulators"] = json::array();

    json e;
    e["peak_volume_num"] = adsr.peak_volume_num;
    e["sustain_volume_num"] = adsr.sustain_volume_num;
    e["volume_denom"] = adsr.volume_denom;
    e["attack_length"] = adsr.attack_length/(double)samplerate;
    e["decay_length"] = adsr.decay_length/(double)samplerate;
    e["release_length"] = adsr.release_length/(double)samplerate;
    j["envelope"] = e;

    for(const auto& o: modulators)
    {
        json m;
        m["type"] = osc_strings[(unsigned)o.get_type()];
        int64_t amp_num, amp_denom;
        uint64_t period_num, period_denom;
        o.get_amplitude(amp_num, amp_denom);
        o.get_period(period_num, period_denom);
        m["amp_num"] = amp_num;
        m["amp_denom"] = amp_denom;
        m["period_num"] = period_num;
        m["period_denom"] = period_denom;

        j["modulators"].push_back(m);
    }
    return j;
}

// The envelope is automatically converted to the correct samplerate.
bool synth_state::deserialize(const json& j, uint64_t samplerate)
{
    modulators.clear();

    try
    {
        j.at("name").get_to(name);
        std::string carrier_str = j.at("carrier").get<std::string>();
        int carrier_i = find_string_arg(carrier_str.c_str(), osc_strings,
            sizeof(osc_strings)/sizeof(*osc_strings));
        if(carrier_i < 0) return false;
        carrier = (oscillator_type)carrier_i;
        j.at("polyphony").get_to(polyphony);

        json e = j.at("envelope");
        e.at("peak_volume_num").get_to(adsr.peak_volume_num);
        e.at("sustain_volume_num").get_to(adsr.sustain_volume_num);
        e.at("volume_denom").get_to(adsr.volume_denom);

        double attack_length, decay_length, release_length;
        e.at("attack_length").get_to(attack_length);
        e.at("decay_length").get_to(decay_length);
        e.at("release_length").get_to(release_length);
        adsr.attack_length = attack_length * samplerate;
        adsr.decay_length = decay_length * samplerate;
        adsr.release_length = release_length * samplerate;

        for(auto& m: j.at("modulators"))
        {
            dynamic_oscillator osc;

            std::string type_str = m.at("type").get<std::string>();
            int type_i = find_string_arg(type_str.c_str(), osc_strings,
                sizeof(osc_strings)/sizeof(*osc_strings));
            if(type_i < 0) return false;
            osc.set_type((oscillator_type)type_i);

            int64_t amp_num, amp_denom;
            uint64_t period_num, period_denom;
            m.at("amp_num").get_to(amp_num);
            m.at("amp_denom").get_to(amp_denom);
            m.at("period_num").get_to(period_num);
            m.at("period_denom").get_to(period_denom);

            osc.set_amplitude(amp_num, amp_denom);
            osc.set_period_fract(period_num, period_denom);

            modulators.push_back(osc);
        }
    }
    catch(...)
    {
        return false;
    }

    return true;
}
