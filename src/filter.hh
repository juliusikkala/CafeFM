/*
    Copyright 2019 Julius Ikkala

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
#ifndef CAFEFM_FILTER_HH
#define CAFEFM_FILTER_HH
#include <cstdint>
#include <cstddef>
#include <vector>
#include "io.hh"

class filter
{
public:
    explicit filter(
        const std::vector<double>& feedforward_coef,
        const std::vector<double>& feedback_coef
    );
    filter(const filter& other);
    filter(filter&& other);

    int32_t push(int32_t sample);

private:
    double feedforward_first;
    std::vector<double> feedforward_coef;
    std::vector<double> feedback_coef;
    unsigned input_head;
    std::vector<double> input;
    unsigned output_head;
    std::vector<double> output;
};

struct filter_state
{
    filter_state();

    enum filter_type
    {
        NONE = 0,
        LOW_PASS,
        HIGH_PASS,
        BAND_PASS,
        BAND_STOP,
    } type;

    double f0;
    double bandwidth;
    unsigned order;

    filter design(uint64_t samplerate);

    json serialize() const;
    bool deserialize(const json& j);
};

#endif
