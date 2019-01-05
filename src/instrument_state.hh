#ifndef CAFEFM_INSTRUMENT_STATE_HH
#define CAFEFM_INSTRUMENT_STATE_HH
#include "fm.hh"
#include "io.hh"

struct instrument_state
{
    std::string name;
    envelope adsr;
    unsigned polyphony;
    fm_synth synth;
    bool write_lock;
    fs::path path;

    instrument_state(uint64_t samplerate = 44100);

    fm_instrument* create_instrument(uint64_t samplerate) const;

    // Samplerate is only used here for ADSR length conversions.
    json serialize(uint64_t samplerate) const;

    // The envelope is automatically converted to the correct samplerate.
    bool deserialize(const json& j, uint64_t samplerate);
};

#endif
