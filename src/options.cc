#include "options.hh"
#include "audio.hh"

options::options()
: system_index(-1), device_index(-1), samplerate(44100), target_latency(0.025)
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
    target_latency = 0.025;

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
