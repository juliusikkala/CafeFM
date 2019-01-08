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
#include "joystick.hh"
#include <algorithm>

joystick::joystick(int device_index)
{
    js = SDL_JoystickOpen(device_index);
    id = SDL_JoystickInstanceID(js);
}

joystick::~joystick()
{
    if(js)
    {
        SDL_JoystickClose(js);
        js = nullptr;
    }
}

bool joystick::handle_event(const SDL_Event& e, change_callback cb)
{
    unsigned axes = SDL_JoystickNumAxes(js);
    unsigned buttons = SDL_JoystickNumButtons(js);
    switch(e.type)
    {
    case SDL_JOYDEVICEREMOVED:
        if(e.jdevice.which == id) return false;
        break;
    case SDL_JOYAXISMOTION:
        if(e.jaxis.which == id && cb) cb(this, e.jaxis.axis, -1, -1);
        break;
    case SDL_JOYBALLMOTION:
        if(e.jball.which == id && cb)
        {
            if(e.jball.xrel != 0) cb(this, axes+e.jball.ball*2, -1, -1);
            if(e.jball.yrel != 0) cb(this, axes+e.jball.ball*2+1, -1, -1);
        }
        break;
    case SDL_JOYHATMOTION:
        if(e.jhat.which == id && cb)
        {
            uint8_t prev_state = hat_states[e.jhat.hat];
            uint8_t changed = prev_state^e.jhat.value;
            if(changed&1) cb(this, -1, -1, buttons+e.jhat.hat*4);
            if(changed&2) cb(this, -1, -1, buttons+e.jhat.hat*4+1);
            if(changed&4) cb(this, -1, -1, buttons+e.jhat.hat*4+2);
            if(changed&8) cb(this, -1, -1, buttons+e.jhat.hat*4+3);
            hat_states[e.jhat.hat] = e.jhat.value;
        }
        break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        if(e.jbutton.which == id && cb) cb(this, -1, -1, e.jbutton.button);
        break;
    }
    return true;
}

std::string joystick::get_type_name() const
{
    return "Joystick";
}

std::string joystick::get_device_name() const
{
    const char* name = SDL_JoystickName(js);
    return name ? name : "Generic joystick";
}

unsigned joystick::get_axis_1d_count() const
{
    return SDL_JoystickNumAxes(js) + SDL_JoystickNumBalls(js)*2;
}

std::string joystick::get_axis_1d_name(unsigned i) const
{
    unsigned axes = SDL_JoystickNumAxes(js);
    if(i < axes)
        return "Axis " + std::to_string(i);
    i -= axes;
    return "Ball " + std::to_string(i/2) + ((i&1) ? " Y" : " X");
}

axis_1d joystick::get_axis_1d_state(unsigned i) const
{
    axis_1d res;
    res.is_signed = true;

    unsigned axes = SDL_JoystickNumAxes(js);
    if(i < axes)
    {
        res.is_limited = true;
        res.value = std::max(SDL_JoystickGetAxis(js, i)/32767.0, -1.0);
    }
    else
    {
        i -= axes;
        SDL_Point p;
        SDL_JoystickGetBall(js, i/2, &p.x, &p.y);
        SDL_Point old = ball_states[i/2];
        p.x += old.x; p.y += old.y;
        ball_states[i/2] = p;

        int result = (i&1) ? p.y : p.x;

        // TODO: Determine what this should be divided by!
        res.value = result / 1024.0;
    }
    return res;
}

unsigned joystick::get_button_count() const
{
    return SDL_JoystickNumButtons(js) + SDL_JoystickNumHats(js)*4;
}

std::string joystick::get_button_name(unsigned i) const
{
    unsigned buttons = SDL_JoystickNumButtons(js);
    if(i < buttons)
        return "Button " + std::to_string(i);
    i -= buttons;
    const char* hat_button_names[] = {"up", "right", "down", "left"};
    return "Hat " + std::to_string(i/4) + " " + hat_button_names[i%4];
}

unsigned joystick::get_button_state(unsigned i) const
{
    unsigned buttons = SDL_JoystickNumButtons(js);
    if(i < buttons) return SDL_JoystickGetButton(js, i);
    i -= buttons;
    unsigned hat_mask = 1<<(i%4);
    return (SDL_JoystickGetHat(js, i/4) & hat_mask) != 0;
}
