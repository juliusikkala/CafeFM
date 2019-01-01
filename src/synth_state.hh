#ifndef CAFEFM_SYNTH_STATE_HH
#define CAFEFM_SYNTH_STATE_HH
#include "fm.hh"
#include "io.hh"

struct synth_state
{
    std::string name;
    envelope adsr;
    oscillator_type carrier;
    unsigned polyphony;
    std::vector<dynamic_oscillator> modulators;
    bool write_lock;
    fs::path path;

    synth_state(uint64_t samplerate = 44100);

    basic_fm_synth* create_synth(uint64_t samplerate) const;

    // Samplerate is only used here for ADSR length conversions.
    json serialize(uint64_t samplerate) const;

    // The envelope is automatically converted to the correct samplerate.
    bool deserialize(const json& j, uint64_t samplerate);
};

#endif
