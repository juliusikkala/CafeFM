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
#include "controller.hh"
#include <stdexcept>
#define outofbounds(i) \
    throw std::runtime_error("Index " + std::to_string(i) + " out of bounds");

controller::~controller() {}

bool controller::handle_event(const SDL_Event&, change_callback)
{
    return true;
}

bool controller::poll(change_callback)
{
    return true;
}


bool controller::assign_bind_on_use() const
{
    return true;
}

bool controller::potentially_inactive() const
{
    return false;
}

std::string controller::get_type_name() const { return "Unknown"; }
std::string controller::get_device_name() const { return "Unknown"; }

unsigned controller::get_axis_count() const { return 0; }
std::string controller::get_axis_name(unsigned i) const { outofbounds(i); }
axis controller::get_axis_state(unsigned i) const { outofbounds(i); }

unsigned controller::get_button_count() const { return 0; }
std::string controller::get_button_name(unsigned i) const { outofbounds(i); }
unsigned controller::get_button_state(unsigned i) const { outofbounds(i); }
