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
#ifndef CAFEFM_INSTRUMENT_STATE_HH
#define CAFEFM_INSTRUMENT_STATE_HH
#include "fm.hh"
#include "io.hh"
#include "filter.hh"

struct instrument_state
{
    std::string name;
    envelope adsr;
    unsigned polyphony;
    fm_synth synth;
    double tuning_frequency;
    bool write_lock;
    fs::path path;
    filter_state filter;

    instrument_state(uint64_t samplerate = 44100);

    fm_instrument* create_instrument(uint64_t samplerate) const;

    // Samplerate is only used here for ADSR length conversions.
    json serialize(uint64_t samplerate) const;

    // The envelope is automatically converted to the correct samplerate.
    bool deserialize(const json& j, uint64_t samplerate);
};

#endif
