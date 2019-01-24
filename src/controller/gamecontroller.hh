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
#ifndef CAFEFM_CONTROLLER_GAMECONTROLLER_HH
#define CAFEFM_CONTROLLER_GAMECONTROLLER_HH
#include "controller.hh"
#include <memory>

class gamecontroller: public controller
{
public:
    gamecontroller(int device_index);
    gamecontroller(const gamecontroller& other) = delete;
    gamecontroller(gamecontroller&& other) = delete;
    ~gamecontroller();

    bool handle_event(const SDL_Event& e, change_callback cb = {}) override;

    std::string get_type_name() const override;
    std::string get_device_name() const override;

    unsigned get_axis_count() const override;
    std::string get_axis_name(unsigned i) const override;
    axis get_axis_state(unsigned i) const override;

    unsigned get_button_count() const override;
    std::string get_button_name(unsigned i) const override;
    unsigned get_button_state(unsigned i) const override;

private:
    SDL_GameController* gc;
    SDL_JoystickID id;
};

#endif
