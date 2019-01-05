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
