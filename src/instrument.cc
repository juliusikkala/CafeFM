#include "instrument.hh"
#include "func.hh"
#include <cstddef>
#include <cstdio>
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

instrument::instrument(uint64_t samplerate)
: tuning_offset(0), volume(0.5), samplerate(samplerate)
{
    voices.resize(1, {false, false, 0, 0, 0.0, 0});
    adsr.set_volume(1.0f, 0.5f);
    adsr.set_curve(0.1f, 0.2f, 0.1f, samplerate);
}

instrument::~instrument() {}

void instrument::set_tuning(double offset)
{
    tuning_offset = offset;
    refresh_all_voices();
}

double instrument::get_tuning() const
{
    return tuning_offset;
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

void instrument::press_voice(voice_id id, int semitone)
{
    voices[id].enabled = true;
    voices[id].pressed = true;
    voices[id].press_timer = adsr.attack_length + adsr.decay_length;
    voices[id].release_timer = adsr.release_length;
    voices[id].semitone = semitone;
    refresh_voice(id);
}

void instrument::release_voice(voice_id id)
{
    voices[id].pressed = false;
}

void instrument::set_voice_tuning(voice_id id, double offset)
{
    voices[id].tuning = offset;
    refresh_voice(id);
}

double instrument::get_voice_tuning(voice_id id)
{
    return voices[id].tuning;
}

void instrument::reset_all_voice_tuning()
{
    for(voice& v: voices) v.tuning = 0.0;
    refresh_all_voices();
}

void instrument::set_polyphony(unsigned n)
{
    voices.resize(n, {false, false, 0, 0, 0.0, 0});
    handle_polyphony(n);
}

unsigned instrument::get_polyphony() const
{
    return voices.size();
}

void instrument::set_envelope(const envelope& adsr)
{
    this->adsr = adsr;
}

envelope instrument::get_envelope() const
{
    return adsr;
}

void instrument::set_volume(double volume)
{
    this->volume = volume;
}

double instrument::get_volume() const
{
    return volume;
}

double instrument::get_frequency(voice_id id) const
{
    return (440.0 + voices[id].tuning + tuning_offset) * pow(2.0, voices[id].semitone/12.0);
}

void instrument::get_voice_volume(voice_id id, int64_t& num, int64_t& denom)
{
    voice& v = voices[id];

    denom = adsr.volume_denom;
    if(!v.enabled)
    {
        num = 0;
        return;
    }

    int64_t attack_timer = v.press_timer - adsr.decay_length;
    int64_t decay_timer = v.press_timer;

    if(attack_timer > 0)
        num = lerp(adsr.peak_volume_num, 0, attack_timer, adsr.attack_length);
    else if(decay_timer > 0)
        num = lerp(
            adsr.sustain_volume_num,
            adsr.peak_volume_num,
            decay_timer,
            adsr.decay_length
        );
    else num = adsr.sustain_volume_num;

    if(!v.pressed)
        num = lerp(0, num, v.release_timer, adsr.release_length);

    num *= volume;
}

void instrument::step_voices()
{
    for(voice& v: voices)
    {
        if(!v.enabled) continue;

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
    }
}

void instrument::refresh_all_voices()
{
    for(voice_id id = 0; id < voices.size(); ++id)
        refresh_voice(id);
}
