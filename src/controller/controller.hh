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
#ifndef CAFEFM_CONTROLLER_HH
#define CAFEFM_CONTROLLER_HH
#include "SDL.h"
#include <string>
#include <functional>

struct axis
{
    // If is_signed is set, the values can be negative. The axis should always
    // start from zero. If is_limited is set, the values should be normalized to
    // -1 to 1 or 0 to 1, depending on is_signed.
    bool is_signed, is_limited;
    float value;
};

class controller
{
public:
    using change_callback = std::function<void(
        controller* c, int axis_index, int button_index
    )>;

    virtual ~controller();
    // These should return false if controller is disconnected.
    // handle_event is allowed to (and should) ignore unrelated events.
    virtual bool handle_event(const SDL_Event& e, change_callback cb = {});
    virtual bool poll(change_callback cb = {});

    // If false, input binds should be assigned with a drop-down instead of
    // waiting for the user to use the controller. Useful for overlapping
    // controls. Defaults to true.
    virtual bool assign_bind_on_use() const;

    // If true, this controller should not be auto-picked.
    virtual bool potentially_inactive() const;

    virtual std::string get_type_name() const;
    virtual std::string get_device_name() const;

    virtual unsigned get_axis_count() const;
    virtual std::string get_axis_name(unsigned i) const;
    virtual axis get_axis_state(unsigned i) const;

    virtual unsigned get_button_count() const;
    virtual std::string get_button_name(unsigned i) const;
    virtual unsigned get_button_state(unsigned i) const;

private:
};

#endif
