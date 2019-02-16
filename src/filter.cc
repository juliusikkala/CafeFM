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
#include <cmath>
#include <complex>

namespace
{
    const char* const filter_type_strings[] = {
        "NONE", "LOW_PASS", "HIGH_PASS", "BAND_PASS"
    };

    void butterworth_lowpass(
        unsigned order,
        double cutoff,
        std::vector<double>& d,
        std::vector<double>& c
    ){
        std::vector<std::complex<double>> a;
        cutoff = M_PI * cutoff;
        double sc = sin(cutoff), cc = cos(cutoff);

        a.resize(order, 0);
        for(unsigned i = 0; i < order; ++i)
        {
            double pole = M_PI * (2 * i + 1) / (2 * order);
            std::complex<double> r(-cc, -sc * cos(pole));
            r /= (1.0 + sc * sin(pole));
            for(unsigned j = i; j > 0; --j) a[j] += r*a[j-1];
            a[0] += r;
        }

        d.resize(order+1);
        d[0] = 1.0;
        for(unsigned i = 0; i < order; ++i )
            d[i+1] = a[i].real();

        double scale = 1.0;
        for(unsigned i = 0; i < order/2; ++i)
        {
            double pole = M_PI * (2 * i + 1) / (2 * order);
            scale *= 1.0 + sc * sin(pole);
        }

        sc = sin(cutoff * 0.5f); cc = cos(cutoff * 0.5f);

        if(order & 1) scale *= sc + cc;
        scale = pow(sc, order) / scale;

        c.resize(order+1, scale);
        for(unsigned i = 1; i <= order/2; ++i)
        {
            c[i] = (order-i+1)*c[i-1]/i;
            c[order-i] = c[i];
        }
    }
}

filter::filter(
    const std::vector<double>& feedforward_coef,
    const std::vector<double>& feedback_coef
):  feedforward_coef(feedforward_coef.size()-1),
    feedback_coef(feedback_coef.size()-1),
    input_head(0), input(feedforward_coef.size()-1, 0),
    output_head(0), output(feedback_coef.size()-1, 0)
{
    double feedback_first = feedback_coef[0];
    feedforward_first = feedforward_coef[0]/feedback_first;

    for(unsigned i = 0; i < this->feedforward_coef.size(); ++i)
        this->feedforward_coef[i] = feedforward_coef[i + 1]/feedback_first;
    for(unsigned i = 0; i < this->feedback_coef.size(); ++i)
        this->feedback_coef[i] = -feedback_coef[i + 1]/feedback_first;
}

filter::filter(const filter& other)
:   feedforward_first(other.feedforward_first),
    feedforward_coef(other.feedforward_coef),
    feedback_coef(other.feedback_coef),
    input_head(other.input_head), input(other.input),
    output_head(other.output_head), output(other.output)
{
}

filter::filter(filter&& other)
:   feedforward_first(other.feedforward_first),
    feedforward_coef(std::move(other.feedforward_coef)),
    feedback_coef(std::move(other.feedback_coef)),
    input_head(other.input_head), input(std::move(other.input)),
    output_head(other.output_head), output(std::move(other.output))
{
}

int32_t filter::push(int32_t sample)
{
    double output_sample = feedforward_first * (double)sample;
    unsigned i = 0, j = input_head;
    unsigned first_len = feedforward_coef.size() - j;
    unsigned second_len = feedforward_coef.size() - first_len;
    unsigned e = feedforward_coef.size()-1;

    for(; i < first_len; ++i, ++j)
        output_sample += input[j] * feedforward_coef[e-i];

    j = 0;
    for(; j < second_len; ++i, ++j)
        output_sample += input[j] * feedforward_coef[e-i];

    i = 0; j = output_head;
    first_len = feedback_coef.size() - j;
    second_len = feedback_coef.size() - first_len;
    e = feedback_coef.size()-1;

    for(; i < first_len; ++i, ++j)
        output_sample += output[j] * feedback_coef[e-i];

    j = 0;
    for(; j < second_len; ++i, ++j)
        output_sample += output[j] * feedback_coef[e-i];

    input[input_head++] = sample;
    output[output_head++] = output_sample;

    input_head %= input.size();
    output_head %= output.size();

    return output_sample;
}

filter_state::filter_state()
: type(NONE), f0(200), bandwidth(100), order(8)
{
}

filter filter_state::design(uint64_t samplerate)
{
    double cutoff = f0/(samplerate*0.5);
    std::vector<double> x, y;
    switch(type)
    {
    case NONE:
        return filter({}, {});
    case LOW_PASS:
        butterworth_lowpass(order, cutoff, y, x);
        return filter(x, y);
    default:
        printf("TODO: Should design filter!\n");
        return filter({}, {});
    }
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
