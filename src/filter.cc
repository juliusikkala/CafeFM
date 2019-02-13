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
#include "helpers.hh"
#define DENOM 16

namespace
{
    const char* const filter_type_strings[] = {
        "NONE", "LOW_PASS", "HIGH_PASS", "BAND_PASS"
    };
}

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

filter::filter(const filter& other)
:   feedforward_first(other.feedforward_first),
    feedback_first(other.feedback_first),
    feedforward_coef(other.feedforward_coef),
    feedback_coef(other.feedback_coef),
    input_head(other.input_head), input(other.input),
    output_head(other.output_head), output(other.output)
{
}

filter::filter(filter&& other)
:   feedforward_first(other.feedforward_first),
    feedback_first(other.feedback_first),
    feedforward_coef(std::move(other.feedforward_coef)),
    feedback_coef(std::move(other.feedback_coef)),
    input_head(other.input_head), input(std::move(other.input)),
    output_head(other.output_head), output(std::move(other.output))
{
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

filter_state::filter_state()
: type(NONE), f0(200), bandwidth(100), order(32)
{
}

filter filter_state::design(uint64_t)
{
    printf("TODO: Should design filter!\n");
    return filter({}, {});
}

json filter_state::serialize() const
{
    json j;

    j["type"] = filter_type_strings[(unsigned)type];

    if(type != NONE)
    {
        j["f0"] = f0;
        j["bandwidth"] = bandwidth;
        j["order"] = order;
    }

    return j;
}

bool filter_state::deserialize(const json& j)
{
    std::string type_str = j.at("type").get<std::string>();
    int type_i = find_string_arg(
        type_str.c_str(),
        filter_type_strings,
        sizeof(filter_type_strings)/sizeof(*filter_type_strings)
    );
    if(type_i < 0) type = NONE;
    else type = (enum filter_type)type_i;

    f0 = j.value("f0", 200.0);
    bandwidth = j.value("bandwidth", 100.0);
    order = j.value("order", 32);
    return true;
}
