#include "io.hh"
#include "bindings.hh"
#include <cstdio>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <regex>
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

fs::path get_bindings_path()
{
    // TODO: Create this in user's config folder. SDL has a function for getting
    // the needed path.
    return fs::path{"bindings"};
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

    write_json_file(get_bindings_path()/filename, bindings_json);
}

std::vector<bindings> load_all_bindings()
{
    std::vector<bindings> binds;
    for(fs::directory_entry& x: fs::directory_iterator(get_bindings_path()))
    {
        try
        {
            bindings new_bindings;
            new_bindings.deserialize(read_json_file(x.path()));
            binds.push_back(new_bindings);
        }
        // Quietly swallow failed files.
        catch(...) {}
    }
    return binds;
}
