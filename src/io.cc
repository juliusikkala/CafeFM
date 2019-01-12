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
#include "io.hh"
#include "bindings.hh"
#include "options.hh"
#include "instrument_state.hh"
#include "encoder.hh"
#include "SDL.h"
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <set>
#include <ctime>
#include <unordered_set>
#include <boost/algorithm/string.hpp>

namespace
{

std::string read_text_file(const std::string& path)
{
    FILE* f = fopen(path.c_str(), "rb");

    if(!f) throw std::runtime_error("Unable to open " + path);

    fseek(f, 0, SEEK_END);
    size_t sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* data = new char[sz];
    if(fread(data, 1, sz, f) != sz)
    {
        fclose(f);
        delete [] data;
        throw std::runtime_error("Unable to read " + path);
    }
    fclose(f);
    std::string ret(data, sz);

    delete [] data;
    return ret;
}

void write_binary_file(
    const std::string& path,
    const uint8_t* data,
    size_t size
){
    FILE* f = fopen(path.c_str(), "wb");

    if(!f) throw std::runtime_error("Unable to open " + path);

    if(fwrite(data, 1, size, f) != size)
    {
        fclose(f);
        throw std::runtime_error("Unable to write " + path);
    }
    fclose(f);
}

void write_text_file(const std::string& path, const std::string& content)
{
    write_binary_file(path, (uint8_t*)content.c_str(), content.size());
}

fs::path get_writable_path()
{
    static bool has_path = false;
    static fs::path path;
    if(!has_path)
    {
        char* path_str = SDL_GetPrefPath("jji.fi", "CafeFM");
        path = path_str;
        SDL_free(path_str);
        path = path.make_preferred();
        has_path = true;
    }

    return path;
}

fs::path get_writable_bindings_path()
{
    fs::path path = get_writable_path()/"bindings";
    if(!fs::exists(path)) fs::create_directory(path);
    return path;
}

fs::path get_writable_instruments_path()
{
    fs::path path = get_writable_path()/"instruments";
    if(!fs::exists(path)) fs::create_directory(path);
    return path;
}

fs::path get_writable_recordings_path()
{
    fs::path path = get_writable_path()/"recordings";
    if(!fs::exists(path)) fs::create_directory(path);
    return path;
}

std::set<fs::path> get_readonly_paths()
{
    static bool has_path = false;
    static fs::path path;
    if(!has_path)
    {
        char* path_str = SDL_GetBasePath();
        path = path_str;
        SDL_free(path_str);
        path = path.make_preferred();
        has_path = true;
    }

    std::set<fs::path> paths;
    paths.insert(path);
    // This is mostly for testing, but useful if you don't want to install
    // CafeFM.
    paths.insert(".");
#ifdef DATA_DIRECTORY
    paths.insert(fs::path{DATA_DIRECTORY});
#endif

    return paths;
}

std::set<fs::path> get_readonly_bindings_paths()
{
    std::set<fs::path> paths = get_readonly_paths();
    std::set<fs::path> bindings_paths;
    for(const fs::path& path: paths) bindings_paths.insert(path/"bindings");
    return bindings_paths;
}

std::set<fs::path> get_readonly_instruments_paths()
{
    std::set<fs::path> paths = get_readonly_paths();
    std::set<fs::path> bindings_paths;
    for(const fs::path& path: paths) bindings_paths.insert(path/"instruments");
    return bindings_paths;
}

std::string string_hash(const std::string& str)
{
    size_t hash = std::hash<std::string>{}(str);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(sizeof(size_t)*2)
       << std::hex << hash;
    return ss.str();
}

std::string get_timestamp()
{
    time_t now = time(0);
    char buf[80];
    strftime(buf, sizeof(buf), "%F-%H.%M.%S", localtime(&now));

    return buf;
}

}

void write_json_file(const fs::path& path, const json& j)
{
    write_text_file(path.string(), j.dump(2));
}

json read_json_file(const fs::path& path)
{
    return json::parse(read_text_file(path.string()));
}

void open_bindings_folder()
{
    // TODO: Make this safer than relying on system()
    fs::path path = get_writable_bindings_path();
#ifdef USE_XDG
    system(("xdg-open "+path.string()).c_str());
#else
    system(("explorer "+path.string()).c_str());
#endif
}

void open_instruments_folder()
{
    // TODO: Make this safer than relying on system()
    fs::path path = get_writable_instruments_path();
#ifdef USE_XDG
    system(("xdg-open "+path.string()).c_str());
#else
    system(("explorer "+path.string()).c_str());
#endif
}

void open_recordings_folder()
{
    // TODO: Make this safer than relying on system()
    fs::path path = get_writable_recordings_path();
#ifdef USE_XDG
    system(("xdg-open "+path.string()).c_str());
#else
    system(("explorer "+path.string()).c_str());
#endif
}

std::string make_filename_safe(const std::string& name)
{
    std::string fixed = name;
    fixed.erase(std::remove_if(
        fixed.begin(), fixed.end(),
        [](char c){ return strchr("/<>:\"\'\\|?*.", c) || c <= 31; }
    ), fixed.end());
    boost::trim(fixed);

    // Thanks Windows!
    static const std::unordered_set<std::string> banned_names = {
        "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
        "LPT6", "LPT7", "LPT8", "LPT9"
    };

    if(banned_names.count(fixed))
        fixed = "Thanks Windows!";

    std::regex whitespace("\\s");
    fixed = std::regex_replace(fixed, whitespace, "_");
    return fixed;
}

void write_bindings(bindings& b)
{
    json bindings_json = b.serialize();
    std::string name = bindings_json.at("name").get<std::string>();
    fs::path filename(
        make_filename_safe(name) + "_" + string_hash(name) + ".bnd"
    );
    fs::path path = get_writable_bindings_path()/filename;
    b.set_path(path);

    write_json_file(path, bindings_json);
}

void remove_bindings(const bindings& b)
{
    // Safety checks, just in case something goes very wrong.
    fs::path writable_path = get_writable_bindings_path();
    fs::path b_path = b.get_path();
    if(
        b_path.extension() == ".bnd" &&
        b_path.has_stem() &&
        b_path.parent_path() == writable_path
    ) fs::remove(b_path);
}

std::vector<bindings> load_all_bindings()
{
    std::vector<bindings> binds;

    std::set<fs::path> ro_paths = get_readonly_bindings_paths();
    std::set<fs::path> paths = ro_paths;
    paths.insert(get_writable_bindings_path());

    for(const fs::path& p: paths)
    {
        if(!fs::exists(p) || !fs::is_directory(p)) continue;
        for(fs::directory_entry& x: fs::directory_iterator(p))
        {
            try
            {
                bindings new_bindings;
                new_bindings.deserialize(read_json_file(x.path()));
                new_bindings.set_path(x.path());
                if(ro_paths.count(p)) new_bindings.set_write_lock(true);
                binds.push_back(new_bindings);
            }
            // Quietly swallow failed files.
            catch(...) {}
        }
    }

    return binds;
}

void write_instrument(uint64_t samplerate, instrument_state& ins)
{
    json ins_json = ins.serialize(samplerate);
    fs::path filename(
        make_filename_safe(ins.name) + "_" + string_hash(ins.name) + ".ins"
    );
    fs::path path = get_writable_instruments_path()/filename;
    ins.path = path;
    ins.write_lock = false;

    write_json_file(path, ins_json);
}

void remove_instrument(const instrument_state& ins)
{
    // Safety checks, just in case something goes very wrong.
    fs::path writable_path = get_writable_instruments_path();
    fs::path path = ins.path;
    if(
        path.extension() == ".ins" &&
        path.has_stem() &&
        path.parent_path() == writable_path
    ) fs::remove(path);
}

std::vector<instrument_state> load_all_instruments(uint64_t samplerate)
{
    std::vector<instrument_state> instruments;

    std::set<fs::path> ro_paths = get_readonly_instruments_paths();
    std::set<fs::path> paths = ro_paths;
    paths.insert(get_writable_instruments_path());

    for(const fs::path& p: paths)
    {
        if(!fs::exists(p) || !fs::is_directory(p)) continue;
        for(fs::directory_entry& x: fs::directory_iterator(p))
        {
            try
            {
                instrument_state ins;
                ins.deserialize(read_json_file(x.path()), samplerate);
                ins.path = x.path();
                if(ro_paths.count(p)) ins.write_lock = true;
                instruments.push_back(ins);
            }
            // Quietly swallow failed files.
            catch(...) {}
        }
    }

    return instruments;
}

void write_recording(const encoder& enc)
{
    static const char* extension[] = { ".wav", ".ogg", ".flac" };
    fs::path filename(get_timestamp() + extension[(int)enc.get_format()]);
    fs::path path = get_writable_recordings_path()/filename;
    write_binary_file(path.string(), enc.get_data(), enc.get_data_size());
}

void write_options(const options& opts)
{
    write_json_file(get_writable_path()/"options.json", opts.serialize());
}

void load_options(options& opts)
{
    try
    {
        opts.deserialize(read_json_file(get_writable_path()/"options.json"));
    }
    // Failure is fine, just reset options.
    catch(...)
    {
        opts = options();
    }
}
