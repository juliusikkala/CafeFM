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

class filter
{
public:
    explicit filter(
        const std::vector<float>& feedforward_coef,
        const std::vector<float>& feedback_coef
    );
    filter(const filter& other) = delete;
    filter(filter&& other) = delete;

    int32_t push(int32_t sample);

private:
    int32_t feedforward_first;
    int32_t feedback_first;
    std::vector<int32_t> feedforward_coef;
    std::vector<int32_t> feedback_coef;
    unsigned input_head;
    std::vector<int32_t> input;
    unsigned output_head;
    std::vector<int32_t> output;
};

#endif
