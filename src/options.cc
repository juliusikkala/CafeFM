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
#include "options.hh"
#include "audio.hh"

options::options()
: system_index(-1), device_index(-1), samplerate(44100), target_latency(0.030)
{}

json options::serialize() const
{
    json j;
    j["system"] = 
        system_index < 0 ? "" :
        audio_output::get_available_systems()[system_index];
    j["device"] = 
        device_index < 0 || system_index < 0 ? "":
        audio_output::get_available_devices(system_index)[device_index];
    j["samplerate"] = samplerate;
    j["target_latency"] = target_latency;
    return j;
}

bool options::deserialize(const json& j)
{
    system_index = -1;
    device_index = -1;
    samplerate = 44100;
    target_latency = 0.030;

    try
    {
        std::string system_name = j.at("system").get<std::string>();
        auto systems = audio_output::get_available_systems();
        for(unsigned i = 0; i < systems.size(); ++i)
        {
            if(systems[i] == system_name)
            {
                system_index = i;
                break;
            }
        }

        if(system_index != -1)
        {
            std::string device_name = j.at("device").get<std::string>();
            auto devices = audio_output::get_available_devices(system_index);
            for(unsigned i = 0; i < devices.size(); ++i)
            {
                if(devices[i] == device_name)
                {
                    device_index = i;
                    break;
                }
            }
        }

        j.at("samplerate").get_to(samplerate);
        j.at("target_latency").get_to(target_latency);
    }
    catch(...)
    {
        return false;
    }

    return true;
}

bool options::operator!=(const options& other) const
{
    return memcmp(this, &other, sizeof(options)) != 0;
}
