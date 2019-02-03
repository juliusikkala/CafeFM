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
#ifndef CAFEFM_CONTROLLER_MIDI_HH
#define CAFEFM_CONTROLLER_MIDI_HH
#include "controller.hh"
#include "RtMidi.h"
#include <memory>
#include <string>
#include <map>

class midi_controller;
class bindings;
class midi_context
{
friend class midi_controller;
public:
    midi_context();
    ~midi_context();

    bool is_available() const;

    std::vector<midi_controller*> discover();

    static bindings generate_default_midi_bindings();

private:
    std::map<std::string /* name */, bool /* connected */> status;
    std::unique_ptr<RtMidiIn> in;
};

class midi_controller: public controller
{
friend class midi_context;
private:
    midi_controller(midi_context& ctx, unsigned port);
public:
    midi_controller(const midi_controller& other) = delete;
    midi_controller(midi_controller&& other) = delete;
    ~midi_controller();

    bool poll(change_callback cb = {}, bool active = true) override;

    bool potentially_inactive() const override;

    std::string get_type_name() const override;
    std::string get_device_name() const override;

    unsigned get_axis_count() const override;
    std::string get_axis_name(unsigned i) const override;
    axis get_axis_state(unsigned i) const override;

    unsigned get_button_count() const override;
    std::string get_button_name(unsigned i) const override;
    unsigned get_button_state(unsigned i) const override;

private:
    midi_context* ctx;
    RtMidiIn in;
    std::string name;

    std::vector<uint8_t> note_velocity;
    std::vector<uint8_t> note_aftertouch;
    std::vector<uint16_t> control_axes;
    std::vector<bool> control_buttons;
    uint8_t program;
    uint16_t pitch_wheel;
};
#endif
