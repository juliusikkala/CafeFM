#include "audio.hh"
#include <map>

namespace
{

std::vector<std::pair<const PaHostApiInfo*, PaHostApiIndex>> get_host_apis()
{
    static bool cached = false;
    static std::vector<std::pair<const PaHostApiInfo*, PaHostApiIndex>> res;
    if(!cached)
    {
        int api_count = Pa_GetHostApiCount();
        for(int i = 0; i < api_count; ++i)
        {
            const PaHostApiInfo* info = Pa_GetHostApiInfo(i);
            if(!info || info->deviceCount == 0) continue;
            res.emplace_back(Pa_GetHostApiInfo(i), i);
        }
        cached = true;
    }
    return res;
}

std::vector<std::pair<const PaDeviceInfo*, PaDeviceIndex>> get_devices(
    int system_index
){
    static std::map<
        int,
        std::vector<std::pair<const PaDeviceInfo*, PaDeviceIndex>>
    > cache;
    auto it = cache.find(system_index);
    if(it == cache.end())
    {
        const PaHostApiInfo* api_info;
        PaHostApiIndex index;
        if(system_index >= 0)
        {
            auto pair = get_host_apis()[system_index];
            api_info = pair.first; index = pair.second;
        }
        else
        {
            index = Pa_GetDeviceInfo(Pa_GetDefaultOutputDevice())->hostApi;
            api_info = Pa_GetHostApiInfo(index);
        }

        std::vector<std::pair<const PaDeviceInfo*, PaDeviceIndex>> res;
        int device_count = Pa_GetDeviceCount();
        for(int i = 0; i < device_count; ++i)
        {
            const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
            if(!info || info->hostApi != index || info->maxOutputChannels == 0)
                continue;
            if(i == api_info->defaultOutputDevice)
                res.insert(res.begin(), std::make_pair(info, i));
            else res.emplace_back(info, i);
        }
        cache[system_index] = res;
        return res;
    }
    return it->second;
}

std::vector<uint64_t> get_samplerates(
    PaDeviceIndex index,
    PaTime target_latency = 0.0,
    int channels = 1,
    PaSampleFormat format = paInt32
){
    static auto cmp = [](
        const PaStreamParameters& a,
        const PaStreamParameters& b
    ){
        return memcmp(&a, &b, sizeof(PaStreamParameters)) < 0;
    };
    static std::map<
        PaStreamParameters,
        std::vector<uint64_t>,
        decltype(cmp)
    > cache(cmp);

    PaStreamParameters output;
    output.device = index;
    output.channelCount = channels;
    output.sampleFormat = format;
    output.suggestedLatency = target_latency;
    output.hostApiSpecificStreamInfo = nullptr;

    auto it = cache.find(output);
    if(it == cache.end())
    {
        constexpr uint64_t try_samplerates[] = {44100, 48000, 96000, 192000};
        std::vector<uint64_t> found_samplerates;

        for(uint64_t samplerate: try_samplerates)
        {

            if(
                Pa_IsFormatSupported(
                    nullptr, &output, samplerate
                ) == paFormatIsSupported
            ) found_samplerates.push_back(samplerate);
        }
        cache[output] = found_samplerates;

        return found_samplerates;
    }
    return it->second;
}

}

audio_output::~audio_output()
{
    Pa_CloseStream(stream);
}

void audio_output::start()
{
    Pa_StartStream(stream);
}

void audio_output::stop()
{
    Pa_StopStream(stream);
}

unsigned audio_output::get_samplerate() const
{
    return samplerate;
}

std::vector<const char*> audio_output::get_available_systems()
{
    std::vector<const char*> systems;
    auto apis = get_host_apis();
    for(auto pair: apis) systems.push_back(pair.first->name);
    return systems;
}

std::vector<const char*> audio_output::get_available_devices(
    int system_index
){
    std::vector<const char*> res;
    auto devices = get_devices(system_index);
    for(auto pair: devices) res.push_back(pair.first->name);
    return res;
}

std::vector<uint64_t> audio_output::get_available_samplerates(
    int system_index, int device_index, double target_latency
){
    const PaDeviceInfo* info;
    PaDeviceIndex index;
    if(system_index >= 0)
    {
        auto pair = get_devices(system_index)[device_index];
        info = pair.first;
        index = pair.second;
    }
    else
    {
        index = Pa_GetDefaultOutputDevice();
        info = Pa_GetDeviceInfo(index);
    }

    if(target_latency <= 0) target_latency = info->defaultLowOutputLatency;
    return get_samplerates(index, target_latency);
}

void audio_output::open_stream(
    double target_latency,
    int system_index,
    int device_index,
    PaStreamCallback* callback,
    void* userdata
){
    PaStreamParameters params;

    if(system_index >= 0)
    {
        auto apis = get_host_apis();
        auto devices = get_devices(system_index);
        if(device_index < 0) device_index = 0;
        params.device = devices[device_index].second;
    }
    else params.device = Pa_GetDefaultOutputDevice();

    if(target_latency <= 0)
    {
        target_latency = Pa_GetDeviceInfo(params.device)
            ->defaultLowOutputLatency;
    }

    params.channelCount = 1;
    params.hostApiSpecificStreamInfo = NULL;
    params.sampleFormat = paInt32;
    params.suggestedLatency = target_latency;

    PaError err = Pa_OpenStream(
        &stream,
        nullptr,
        &params,
        samplerate,
        paFramesPerBufferUnspecified,
        paNoFlag,
        callback,
        userdata
    );
    if(err != paNoError)
        throw std::runtime_error(
            "Unable to open stream: " + std::string(Pa_GetErrorText(err))
        );
}
