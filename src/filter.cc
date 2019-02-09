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
#include "filter.hh"
#define DENOM 16

filter::filter(
    const std::vector<float>& feedforward_coef,
    const std::vector<float>& feedback_coef
):  feedforward_coef(feedforward_coef.size()-1),
    feedback_coef(feedback_coef.size()-1),
    input_head(0), input(feedforward_coef.size(), 0),
    output_head(0), output(feedback_coef.size(), 0)
{
    feedforward_first = feedforward_coef[0] * (1 << DENOM);
    if(feedback_coef.size() == 0) feedback_first = 1 << DENOM;
    else feedback_first = (1.0 / feedback_coef[0]) * (1 << DENOM);

    for(unsigned i = 0; i < feedforward_coef.size(); ++i)
        this->feedforward_coef[i] = feedforward_coef[i + 1] * (1 << DENOM);
    for(unsigned i = 0; i < feedback_coef.size(); ++i)
        this->feedback_coef[i] = -feedback_coef[i + 1] * (1 << DENOM);
}

int32_t filter::push(int32_t sample)
{
    int64_t output_sample = sample;
    unsigned i = 0, j = input_head + 1;
    unsigned first_len = feedforward_coef.size() - input_head;
    unsigned second_len = feedforward_coef.size() - first_len;

    for(; i < first_len; ++i, ++j)
        output_sample += input[j] * feedforward_coef[i];

    j = 0;
    for(; j < second_len; ++i, ++j)
        output_sample += input[j] * feedforward_coef[i];

    i = 0; j = output_head + 1;
    first_len = feedback_coef.size() - output_head;
    second_len = feedback_coef.size() - first_len;

    for(; i < first_len; ++i, ++j)
        output_sample += output[j] * feedback_coef[i];

    j = 0;
    for(; j < second_len; ++i, ++j)
        output_sample += output[j] * feedback_coef[i];

    output_sample += feedforward_first * sample;
    output_sample >>= DENOM;
    output_sample = (output_sample * feedback_first) >> DENOM;

    input[input_head++] = sample;
    output[output_head++] = output_sample;
    return output_sample;
}
