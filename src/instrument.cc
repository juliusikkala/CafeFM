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
#include "instrument.hh"
#include "func.hh"
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <climits>
#include <cmath>

void envelope::set_volume(
    double peak_volume,
    double sustain_volume,
    int64_t denom
){
    peak_volume_num = peak_volume * denom;
    sustain_volume_num = sustain_volume * denom;
    volume_denom = denom;
}

void envelope::set_curve(
    double attack_length,
    double decay_length,
    double release_length,
    uint64_t samplerate
){
    this->attack_length = attack_length * samplerate;
    this->decay_length = decay_length * samplerate;
    this->release_length = release_length * samplerate;
}

envelope envelope::convert(
    uint64_t cur_samplerate,
    uint64_t new_samplerate
) const
{
    envelope c(*this);
    c.attack_length = new_samplerate * attack_length / cur_samplerate;
    c.decay_length = new_samplerate * decay_length / cur_samplerate;
    c.release_length = new_samplerate * release_length / cur_samplerate;
    return c;
}

bool envelope::operator==(const envelope& other) const
{
    return memcmp(&other, this, sizeof(envelope)) == 0;
}

instrument::instrument(uint64_t samplerate)
:   base_frequency(440), volume_denom(1<<20), samplerate(samplerate)
{
    voices.resize(1, {false, false, 0, 0, 0, 0, 0});
    adsr.set_volume(1.0f, 0.5f);
    set_volume(0.5f);
    set_max_volume_skip(32);
    adsr.set_curve(0.07f, 0.2f, 0.05f, samplerate);
}

instrument::~instrument() {}

void instrument::set_tuning(double base_frequency)
{
    if(this->base_frequency == base_frequency) return;
    this->base_frequency = base_frequency;
    refresh_all_voices();
}

double instrument::get_tuning() const
{
    return base_frequency;
}

uint64_t instrument::get_samplerate() const
{
    return samplerate;
}

instrument::voice_id instrument::press_voice(int semitone)
{
    uint64_t closest_release = UINT64_MAX;
    voice_id closest_index = voices.size()-1;
    for(size_t i = 0; i < voices.size(); ++i)
    {
        if(!voices[i].enabled)
        {
            closest_index = i;
            break;
        }
        else
        {
            if(voices[i].release_timer < closest_release)
            {
                closest_release = voices[i].release_timer;
                closest_index = i;
            }
        }
    }

    press_voice(closest_index, semitone);
    return closest_index;
}

void instrument::press_voice(voice_id id, int semitone, double volume)
{
    voices[id].enabled = true;
    voices[id].pressed = true;
    voices[id].press_timer = adsr.attack_length + adsr.decay_length;
    voices[id].release_timer = adsr.release_length;
    voices[id].semitone = semitone;
    voices[id].volume_num = volume_denom * volume;
    voices[id].volume = 0;
    reset_voice(id);
}

void instrument::set_voice_volume(voice_id id, double volume)
{
    voices[id].volume_num = volume_denom * volume;
}

void instrument::release_voice(voice_id id)
{
    voices[id].pressed = false;
}

void instrument::release_all_voices()
{
    for(auto& v: voices) v.pressed = false;
}

void instrument::set_polyphony(unsigned n)
{
    if(voices.size() == n) return;
    voices.resize(n, {false, false, 0, 0, 0, 0, 0});
    handle_polyphony(n);
}

unsigned instrument::get_polyphony() const
{
    return voices.size();
}

void instrument::set_envelope(const envelope& adsr)
{
    if(this->adsr == adsr) return;

    envelope old_adsr = this->adsr;
    this->adsr = adsr;
    uint64_t old_pt = old_adsr.attack_length + old_adsr.decay_length;
    uint64_t old_rt = old_adsr.release_length;
    if(old_pt == 0 || old_rt == 0) return;

    uint64_t pt = adsr.attack_length + adsr.decay_length;
    uint64_t rt = adsr.release_length;

    // Adjust timers accordingly.
    for(voice& v: voices)
    {
        v.press_timer = pt*v.press_timer/old_pt;
        v.release_timer = rt*v.release_timer/old_rt;
    }
}

envelope instrument::get_envelope() const
{
    return adsr;
}

void instrument::set_volume(double volume)
{
    volume_num = volume*volume_denom;
}

void instrument::set_max_safe_volume()
{
    set_volume(1.0/voices.size());
}

double instrument::get_volume() const
{
    return volume_num/(double)volume_denom;
}

void instrument::set_max_volume_skip(double max_volume_skip)
{
    this->max_volume_skip = max_volume_skip * volume_denom / samplerate;
    if(this->max_volume_skip <= 0) this->max_volume_skip = 1;
}

void instrument::copy_state(const instrument& other)
{
    unsigned polyphony = voices.size();
    voices = other.voices;
    voices.resize(polyphony, {false, false, 0, 0, 0, 0, 0});
    for(voice& v: voices)
    {
        v.press_timer = samplerate * v.press_timer / other.samplerate;
        v.release_timer = samplerate * v.release_timer / other.samplerate;
    }

    adsr = other.adsr.convert(other.samplerate, samplerate);
    base_frequency = other.base_frequency;
    volume_num = other.volume_num;
    volume_denom = other.volume_denom;
    max_volume_skip = other.max_volume_skip;

    // Reset all voices
    for(voice_id id = 0; id < voices.size(); ++id)
        reset_voice(id);
}

double instrument::get_frequency(voice_id id) const
{
    return base_frequency * pow(2.0, voices[id].semitone/12.0);
}

void instrument::get_voice_volume(voice_id id, int64_t& num, int64_t& denom)
{
    voice& v = voices[id];
    denom = volume_denom;
    num = v.volume;
}

void instrument::update_voice_volume(voice& v)
{
    if(!v.enabled)
    {
        v.volume = 0;
        return;
    }

    int64_t target_volume = 0;

    int64_t attack_timer = v.press_timer - adsr.decay_length;
    int64_t decay_timer = v.press_timer;

    if(attack_timer > 0)
    {
        target_volume = lerp(
            adsr.peak_volume_num, 0, attack_timer, adsr.attack_length
        );
    }
    else if(decay_timer > 0)
    {
        target_volume = lerp(
            adsr.sustain_volume_num,
            adsr.peak_volume_num,
            decay_timer,
            adsr.decay_length
        );
    }
    else target_volume = adsr.sustain_volume_num;

    if(!v.pressed)
    {
        target_volume = lerp(
            0, target_volume, v.release_timer, adsr.release_length
        );
    }

    target_volume = v.volume_num * volume_num * target_volume
        / (volume_denom * adsr.volume_denom);
    int64_t skip_size = target_volume - v.volume;
    if(skip_size < -max_volume_skip) skip_size = -max_volume_skip;
    if(skip_size > max_volume_skip) skip_size = max_volume_skip;
    v.volume += skip_size;
}

void instrument::step_voice(voice_id id)
{
    voice& v = voices[id];
    if(!v.enabled) return;

    if(v.pressed)
    {
        if(v.press_timer) v.press_timer--;
    }
    else
    {
        if(v.release_timer)
        {
            v.release_timer--;
            if(v.release_timer == 0) v.enabled = false;
        }
    }
    update_voice_volume(v);
}

void instrument::refresh_all_voices()
{
    for(voice_id id = 0; id < voices.size(); ++id)
        refresh_voice(id);
}
