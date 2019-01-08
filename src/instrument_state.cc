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
#include "instrument_state.hh"
#include "helpers.hh"

instrument_state::instrument_state(uint64_t samplerate)
: name("New synth"), polyphony(6), write_lock(false)
{
    adsr.set_volume(1.0f, 0.5f);
    adsr.set_curve(0.07f, 0.2f, 0.05f, samplerate);
}

fm_instrument* instrument_state::create_instrument(uint64_t samplerate) const
{
    fm_instrument* res = new fm_instrument(samplerate);
    res->set_synth(synth);
    res->set_volume(1.0/polyphony);
    res->set_polyphony(polyphony);

    return res;
}

json instrument_state::serialize(uint64_t samplerate) const
{
    json j;
    j["name"] = name;
    j["polyphony"] = polyphony;
    j["synth"] = synth.serialize();

    json e;
    e["peak_volume_num"] = adsr.peak_volume_num;
    e["sustain_volume_num"] = adsr.sustain_volume_num;
    e["volume_denom"] = adsr.volume_denom;
    e["attack_length"] = adsr.attack_length/(double)samplerate;
    e["decay_length"] = adsr.decay_length/(double)samplerate;
    e["release_length"] = adsr.release_length/(double)samplerate;
    j["envelope"] = e;

    return j;
}

// The envelope is automatically converted to the correct samplerate.
bool instrument_state::deserialize(const json& j, uint64_t samplerate)
{
    try
    {
        j.at("name").get_to(name);
        j.at("polyphony").get_to(polyphony);

        synth.deserialize(j.at("synth"));
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
    }
    catch(...)
    {
        return false;
    }

    return true;
}
