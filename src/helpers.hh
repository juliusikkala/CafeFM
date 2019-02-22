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
#ifndef CAFEFM_HELPERS_HH
#define CAFEFM_HELPERS_HH
#include <cstring>
#include <vector>
#include <utility>
#include <string>

#define XSTRINGIFY(a) #a
#define STRINGIFY(a) XSTRINGIFY(a)

inline double lerp(double a, double b, double t) { return (1 - t) * a + t * b; }

template<typename T>
T div_round(T a, T b)
{
    return ((a < 0) ^ (b < 0)) ? ((a - b/2)/b) : ((a + b/2)/b);
}

int find_string_arg(
    const char* str,
    const char* const* strings,
    unsigned string_count
);

std::string generate_semitone_name(int semitone);
std::vector<std::string> generate_note_list(
    int min_semitone, int max_semitone
);

unsigned determine_pffft_compatible_size_min(
    unsigned buffer_size, unsigned a, unsigned b, unsigned c, unsigned d
);

unsigned determine_pffft_compatible_size_max(
    unsigned buffer_size, unsigned a, unsigned b, unsigned c, unsigned d
);

#endif
