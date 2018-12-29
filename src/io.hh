#ifndef CAFEFM_IO_HH
#define CAFEFM_IO_HH
#include "json.hpp"
#include <boost/filesystem.hpp>
using json = nlohmann::json;

namespace fs = boost::filesystem;

void write_json_file(const fs::path& path, const json& j);
json read_json_file(const fs::path& path);

class bindings;

std::string make_filename_safe(const std::string& name);

void write_bindings(const bindings& b);
std::vector<bindings> load_all_bindings();

#endif
