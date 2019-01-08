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
#include "gamecontroller.hh"
#include <algorithm>

gamecontroller::gamecontroller(int device_index)
{
    gc = SDL_GameControllerOpen(device_index);
    SDL_Joystick* joystick = SDL_GameControllerGetJoystick(gc);
    id = SDL_JoystickInstanceID(joystick);
}

gamecontroller::~gamecontroller()
{
    if(gc)
    {
        SDL_GameControllerClose(gc);
        gc = nullptr;
    }
}

bool gamecontroller::handle_event(const SDL_Event& e, change_callback cb)
{
    switch(e.type)
    {
    case SDL_CONTROLLERDEVICEREMOVED:
        if(e.cdevice.which == id) return false;
        break;
    case SDL_CONTROLLERAXISMOTION:
        if(e.caxis.which == id && cb) cb(this, e.caxis.axis, -1, -1);
        break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP:
        if(e.cbutton.which == id && cb) cb(this, -1, -1, e.cbutton.button);
        break;
    }
    return true;
}

std::string gamecontroller::get_type_name() const
{
    return "Game controller";
}

std::string gamecontroller::get_device_name() const
{
    const char* name = SDL_GameControllerName(gc);
    return name ? name : "Generic controller";
}

unsigned gamecontroller::get_axis_1d_count() const
{
    return SDL_CONTROLLER_AXIS_MAX;
}

std::string gamecontroller::get_axis_1d_name(unsigned i) const
{
    return SDL_GameControllerGetStringForAxis((SDL_GameControllerAxis)i);
}

axis_1d gamecontroller::get_axis_1d_state(unsigned i) const
{
    SDL_GameControllerAxis axis = (SDL_GameControllerAxis)i;

    axis_1d res;
    res.is_limited = true;
    switch(axis)
    {
    case SDL_CONTROLLER_AXIS_LEFTX:
    case SDL_CONTROLLER_AXIS_LEFTY:
    case SDL_CONTROLLER_AXIS_RIGHTX:
    case SDL_CONTROLLER_AXIS_RIGHTY:
        res.is_signed = true;
        break;
    default:
        res.is_signed = false;
        break;
    }

    res.value = std::max(SDL_GameControllerGetAxis(gc, axis)/32767.0, -1.0);
    return res;
}

unsigned gamecontroller::get_button_count() const
{
    return SDL_CONTROLLER_BUTTON_MAX;
}

std::string gamecontroller::get_button_name(unsigned i) const
{
    return SDL_GameControllerGetStringForButton((SDL_GameControllerButton)i);
}

unsigned gamecontroller::get_button_state(unsigned i) const
{
    return SDL_GameControllerGetButton(
        gc,
        (SDL_GameControllerButton)i
    );
}
