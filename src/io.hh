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
#ifndef CAFEFM_IO_HH
#define CAFEFM_IO_HH
#include "json.hpp"
#include <boost/filesystem.hpp>
using json = nlohmann::json;

namespace fs = boost::filesystem;

void write_json_file(const fs::path& path, const json& j);
json read_json_file(const fs::path& path);

class bindings;

void open_bindings_folder();
void open_instruments_folder();

std::string make_filename_safe(const std::string& name);

void write_bindings(bindings& b);
void remove_bindings(const bindings& b);
std::vector<bindings> load_all_bindings();

void write_bindings(bindings& b);
void remove_bindings(const bindings& b);
std::vector<bindings> load_all_bindings();

class instrument_state;

void write_instrument(uint64_t samplerate, instrument_state& ins);
void remove_instrument(const instrument_state& ins);
std::vector<instrument_state> load_all_instruments(uint64_t samplerate);

class options;

void write_options(const options& opts);
void load_options(options& opts);

#endif
