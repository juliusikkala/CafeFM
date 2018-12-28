#include "controller.hh"
#include <stdexcept>
#define outofbounds(i) \
    throw std::runtime_error("Index " + std::to_string(i) + " out of bounds");

controller::~controller() {}

bool controller::handle_event(const SDL_Event& e, change_callback cb)
{
    return true;
}

bool controller::poll(change_callback cb)
{
    return true;
}

std::string controller::get_type_name() const { return "Unknown"; }
std::string controller::get_device_name() const { return "Unknown"; }

unsigned controller::get_axis_1d_count() const { return 0; }
std::string controller::get_axis_1d_name(unsigned i) const { outofbounds(i); }
axis_1d controller::get_axis_1d_state(unsigned i) const { outofbounds(i); }

unsigned controller::get_axis_2d_count() const { return 0; }
std::string controller::get_axis_2d_name(unsigned i) const { outofbounds(i); }
axis_2d controller::get_axis_2d_state(unsigned i) const { outofbounds(i); }

unsigned controller::get_button_count() const { return 0; }
std::string controller::get_button_name(unsigned i) const { outofbounds(i); }
bool controller::get_button_state(unsigned i) const { outofbounds(i); }
