#ifndef CAFEFM_OPTIONS_HH
#define CAFEFM_OPTIONS_HH
#include "fm.hh"
#include "io.hh"

struct options
{
    options();

    int system_index;
    int device_index;
    uint64_t samplerate;
    double target_latency;

    json serialize() const;
    bool deserialize(const json& j);

    bool operator!=(const options& other) const;
};

#endif
