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
#include "keyboard.hh"

bool keyboard::handle_event(const SDL_Event& e, change_callback cb)
{
    switch(e.type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if(!e.key.repeat && cb) cb(this, -1, -1, e.key.keysym.scancode);
        break;
    }
    return true;
}

std::string keyboard::get_type_name() const
{
    return "Keyboard";
}

std::string keyboard::get_device_name() const
{
    return "Keyboard";
}

unsigned keyboard::get_button_count() const
{
    return SDL_NUM_SCANCODES;
}

std::string keyboard::get_button_name(unsigned i) const
{
    return SDL_GetKeyName(SDL_GetKeyFromScancode((SDL_Scancode)i));
}

unsigned keyboard::get_button_state(unsigned i) const
{
    const uint8_t* state = SDL_GetKeyboardState(NULL);
    return state[i];
}
