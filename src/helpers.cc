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
#include "helpers.hh"
#include <algorithm>

namespace
{
    int div_round_down(int a, int b)
    {
        return a/b + ((a%b) >> 31);
    }
}

int find_string_arg(
    const char* str,
    const char* const* strings,
    unsigned string_count
){
    for(unsigned i = 0; i < string_count; ++i)
        if(strcmp(str, strings[i]) == 0) return i;
    return -1;
}

std::string generate_semitone_name(int semitone)
{
    int octave_number = 5+div_round_down(semitone-3, 12);
    const char* names[12] = {
        "A", "A♯/B♭", "B", "C", "C♯/D♭", "D",
        "D♯/E♭", "E", "F", "F♯/G♭", "G", "G♯/A♭"
    };
    std::string name = names[(semitone%12+12)%12] + std::to_string(octave_number);
    return name;
}

std::vector<std::string> generate_note_list(
    int min_semitone, int max_semitone
){
    std::vector<std::string> res;
    for(; min_semitone < max_semitone; ++min_semitone)
        res.push_back(generate_semitone_name(min_semitone));
    return res;
}

unsigned determine_pffft_compatible_size_min(
    unsigned buffer_size, unsigned a, unsigned b, unsigned c, unsigned d
) {
    if(d >= buffer_size) return d;

    unsigned out = 0;
    if(b == 0)
    {
        out = determine_pffft_compatible_size_min(buffer_size, a+1, b, c, d*2);
        out = std::min(determine_pffft_compatible_size_min(
            buffer_size, a, b+1, c, d*3
        ), out);
        out = std::min(determine_pffft_compatible_size_min(
            buffer_size, a, b, c+1, d*5
        ), out);
    }
    else if(c == 0)
    {
        out = determine_pffft_compatible_size_min(buffer_size, a, b+1, c, d*3);
        out = std::min(
            determine_pffft_compatible_size_min(buffer_size, a, b, c+1, d*5),
            out
        );
    }
    else
    {
        out = determine_pffft_compatible_size_min(
            buffer_size, a, b, c+1, d*5
        );
    }
    return out;
}

unsigned determine_pffft_compatible_size_max(
    unsigned buffer_size, unsigned a, unsigned b, unsigned c, unsigned d
) {
    unsigned out = d;
    if(b == 0)
    {
        if(d*2 <= buffer_size)
            out = determine_pffft_compatible_size_max(
                buffer_size, a+1, b, c, d*2
            );
        if(d*3 <= buffer_size)
            out = std::max(determine_pffft_compatible_size_max(
                buffer_size, a, b+1, c, d*3
            ), out);
        if(d*5 <= buffer_size)
            out = std::max(determine_pffft_compatible_size_max(
                buffer_size, a, b, c+1, d*5
            ), out);
    }
    else if(c == 0)
    {
        if(d*3 <= buffer_size)
            out = determine_pffft_compatible_size_max(
                buffer_size, a, b+1, c, d*3
            );
        if(d*5 <= buffer_size)
            out = std::max(determine_pffft_compatible_size_max(
                buffer_size, a, b, c+1, d*5
            ), out);
    }
    else
    {
        if(d*5 <= buffer_size)
            out = determine_pffft_compatible_size_max(
                buffer_size, a, b, c+1, d*5
            );
    }
    return out;
}
