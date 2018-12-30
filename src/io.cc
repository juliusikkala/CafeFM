#include "io.hh"
#include "bindings.hh"
#include "SDL.h"
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
#include <set>
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

void write_text_file(const std::string& path, const std::string& content)
{
    FILE* f = fopen(path.c_str(), "wb");

    if(!f) throw std::runtime_error("Unable to open " + path);

    if(fwrite(content.c_str(), 1, content.size(), f) != content.size())
    {
        fclose(f);
        throw std::runtime_error("Unable to write " + path);
    }
    fclose(f);
}

fs::path get_writable_bindings_path()
{
    static bool has_bindings_path = false;
    static fs::path path;
    if(!has_bindings_path)
    {
        char* bindings_path = SDL_GetPrefPath("jji.fi", "CafeFM");
        path = bindings_path;
        SDL_free(bindings_path);

        path /= "bindings";
        path = path.make_preferred();
        has_bindings_path = true;

        if(!fs::exists(path))
        {
            // The writable bindings path doesn't exist, so create it.
            fs::create_directory(path);
        }
    }

    return path;
}

std::set<fs::path> get_readonly_bindings_paths()
{
    static bool has_bindings_path = false;
    static fs::path base_path;
    if(!has_bindings_path)
    {
        char* bindings_path = SDL_GetBasePath();
        base_path = bindings_path;
        SDL_free(bindings_path);

        base_path /= "bindings";
        base_path = base_path.make_preferred();
        has_bindings_path = true;
    }

    std::set<fs::path> paths;
    paths.insert(base_path);
    // This is mostly for testing, but useful if you don't want to install
    // CafeFM.
    paths.insert("bindings");
#ifdef DATA_DIRECTORY
    paths.insert(fs::path{DATA_DIRECTORY}/"bindings");
#endif

    return paths;
}

std::string string_hash(const std::string& str)
{
    size_t hash = std::hash<std::string>{}(str);
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(sizeof(size_t)*2)
       << std::hex << hash;
    return ss.str();
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

void write_bindings(const bindings& b)
{
    json bindings_json = b.serialize();
    std::string name = bindings_json.at("name").get<std::string>();
    fs::path filename(
        make_filename_safe(name) + "_" + string_hash(name) + ".bnd"
    );

    write_json_file(get_writable_bindings_path()/filename, bindings_json);
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
                if(ro_paths.count(p)) new_bindings.set_write_lock(true);
                binds.push_back(new_bindings);
            }
            // Quietly swallow failed files.
            catch(...) {}
        }
    }

    return binds;
}
