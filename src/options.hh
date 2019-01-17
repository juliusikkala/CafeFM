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
#ifndef CAFEFM_OPTIONS_HH
#define CAFEFM_OPTIONS_HH
#include "fm.hh"
#include "io.hh"
#include "encoder.hh"

struct options
{
    options();

    int system_index;
    int device_index;
    uint64_t samplerate;
    double target_latency;
    encoder::format recording_format;
    double recording_quality;
    unsigned initial_window_width;
    unsigned initial_window_height;
    bool start_loop_on_sound;

    json serialize() const;
    bool deserialize(const json& j);

    bool operator!=(const options& other) const;
};

#endif
