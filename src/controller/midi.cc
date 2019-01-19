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
#include "midi.hh"
#include "bindings.hh"
#define CENTER (0x40 << 7)
#define VEL_OFFSET 0x00
#define AFTERTOUCH_OFFSET 0x80
#define CONTROL_AXES_OFFSET 0x100
#define PITCH_WHEEL_OFFSET 0x120

midi_context::midi_context()
{
    try
    {
        in.reset(new RtMidiIn);
    }
    catch(RtMidiError& err)
    {
        in.reset();
        err.printMessage();
    }
}

midi_context::~midi_context()
{
}

bool midi_context::is_available() const
{
    return !!in;
}

std::vector<midi_controller*> midi_context::discover()
{
    std::vector<midi_controller*> controllers;
    std::map<std::string, bool> new_status;
    unsigned ports = in->getPortCount();
    for(unsigned i = 0; i < ports; ++i)
    {
        try
        {
            std::string name = in->getPortName(i);
            new_status[name] = true;
            if(status[name]) continue;
            midi_controller* ctrl = new midi_controller(*this, i);
            controllers.push_back(ctrl);
        }
        catch(RtMidiError& err)
        {
            err.printMessage();
        }
    }
    status = new_status;
    return controllers;
}

bindings midi_context::generate_default_midi_bindings()
{
    bindings binds;
    binds.set_write_lock(true);
    binds.set_name("Generic MIDI default");
    binds.set_target_device_type("MIDI input");
    binds.set_target_device_name("");

    for(int i = 0; i < 128; ++i)
    {
        bind& note = binds.create_new_bind(bind::KEY);
        note.control = bind::AXIS_1D_CONTINUOUS;
        note.axis_1d.index = VEL_OFFSET + i;
        note.axis_1d.invert = false;
        note.axis_1d.threshold = 0;
        note.axis_1d.origin = 0;
        note.key_semitone = i - 69;
    }

    bind& pitch = binds.create_new_bind(bind::FREQUENCY_EXPT);
    pitch.control = bind::AXIS_1D_CONTINUOUS;
    pitch.frequency.max_expt = 6;
    pitch.axis_1d.index = PITCH_WHEEL_OFFSET;
    pitch.axis_1d.invert = false;
    pitch.axis_1d.threshold = 0;
    pitch.axis_1d.origin = 0;

    bind& volume = binds.create_new_bind(bind::VOLUME_MUL);
    volume.control = bind::AXIS_1D_CONTINUOUS;
    volume.volume.max_mul = 0;
    volume.axis_1d.index = CONTROL_AXES_OFFSET + 0x07;
    volume.axis_1d.invert = true;
    volume.axis_1d.threshold = 0;
    volume.axis_1d.origin = 1.0;

    return binds;
}

midi_controller::midi_controller(midi_context& ctx, unsigned port)
: ctx(&ctx)
{
    name = in.getPortName(port);
    in.openPort(port, "CaféFM input");

    // 128 notes in midi.
    note_velocity.resize(128, 0);
    note_aftertouch.resize(128, 0);
    control_axes.resize(32, 0);
    // Start with max volume.
    control_axes[0x07] = 0x3FFF;
    control_buttons.resize(32, 0);
    program = 0;
    pitch_wheel = CENTER;
}

midi_controller::~midi_controller()
{
}

bool midi_controller::poll(change_callback cb)
{
    std::vector<uint8_t> m;
    for(;;)
    {
        m.clear();
        in.getMessage(&m);
        if(m.size() == 0) break;

        uint8_t func = m[0]&0xF0;
        // uint8_t chan = m[0]&0x0F;
        uint8_t d0 = m.size() > 1 ? m[1] : 0;
        uint8_t d1 = m.size() > 2 ? m[2] : 0;

        switch(func)
        {
        case 0x80:
            note_velocity[d0] = 0;
            if(cb) cb(this, VEL_OFFSET + d0, -1, -1);
            break;
        case 0x90:
            note_velocity[d0] = d1;
            if(cb) cb(this, VEL_OFFSET + d0, -1, -1);
            break;
        case 0xA0:
            note_aftertouch[d0] = d1;
            if(cb) cb(this, AFTERTOUCH_OFFSET + d0, -1, -1);
            break;
        case 0xB0:
            if(d0 < 0x20)
            {
                control_axes[d0] &= 0x7F;
                control_axes[d0] |= ((uint16_t)d1)<<7;
                if(cb) cb(this, CONTROL_AXES_OFFSET + d0, -1, -1);
            }
            else if(d0 < 0x40)
            {
                d0 -= 0x20;
                control_axes[d0] &= 0x3F80;
                control_axes[d0] |= d1;
                if(cb) cb(this, CONTROL_AXES_OFFSET + d0, -1, -1);
            }
            else if(d0 < 0x60)
            {
                d0 -= 0x40;
                control_buttons[d0] = d1 >= 64;
                if(cb) cb(this, -1, -1, d0);
            }
            else if(d0 >= 0x7B)
            {
                note_velocity.assign(128, 0);
                for(unsigned i = 0; i < note_velocity.size(); ++i)
                    if(cb) cb(this, VEL_OFFSET + i, -1, -1);
            }
            break;
        case 0xC0:
            program = d0;
            break;
        case 0xD0:
            note_aftertouch.assign(note_aftertouch.size(), d0);
            for(unsigned i = 0; i < note_aftertouch.size(); ++i)
                if(cb) cb(this, AFTERTOUCH_OFFSET + i, -1, -1);
            break;
        case 0xE0:
            pitch_wheel = d0 | (((uint16_t)d1) << 7);
            if(cb) cb(this, PITCH_WHEEL_OFFSET, -1, -1);
            break;
        default:
            break;
        }
    }

    return ctx->status[name];
}

std::string midi_controller::get_type_name() const
{
    return "MIDI input";
}

std::string midi_controller::get_device_name() const
{
    return name;
}

unsigned midi_controller::get_axis_1d_count() const
{
    return
        note_velocity.size() +
        note_aftertouch.size() +
        control_axes.size() +
        1 /* pitch wheel */;
}

std::string midi_controller::get_axis_1d_name(unsigned i) const
{
    if(i < note_velocity.size()) return "Note " + std::to_string(i);
    i -= note_velocity.size();

    if(i < note_aftertouch.size()) return "Aftertouch " + std::to_string(i);
    i -= note_aftertouch.size();

    constexpr const char* const control_names[] = {
        "Bank Select",
        "Modulation",
        "Breath",
        "Continuous 3",
        "Foot",
        "Port. time",
        "Data entry",
        "Volume",
        "Balance",
        "Continuous 9",
        "Pan",
        "Expression",
        "Effect 1",
        "Effect 2"
    };
    if(i < sizeof(control_names)/sizeof(*control_names)) 
        return control_names[i];
    else if(i < control_axes.size())
        return "Continuous " + std::to_string(i);
    return "Pitch";
}

axis_1d midi_controller::get_axis_1d_state(unsigned i) const
{
    axis_1d res;
    res.is_limited = true;
    res.is_signed = false;

    if(i < note_velocity.size())
    {
        res.is_signed = false;
        res.value = note_velocity[i]/(double)0x7F;
        return res;
    }
    i -= note_velocity.size();

    if(i < note_aftertouch.size())
    {
        res.is_signed = false;
        res.value = note_aftertouch[i]/(double)0x7F;
        return res;
    }
    i -= note_aftertouch.size();

    if(i < control_axes.size())
    {
        switch(i)
        {
        case 8:
        case 10:
            res.is_signed = true;
            res.value = (control_axes[i] - CENTER)/(double)0x1F80;
            res.value = std::max(std::min(res.value, 1.0f), -1.0f);
            return res;
        default:
            res.value = control_axes[i]/(double)0x3F80;
            res.value = std::min(res.value, 1.0f);
            return res;
        }
    }

    res.is_signed = true;
    res.value = (pitch_wheel - CENTER)/(double)0x1F80;
    res.value = std::max(std::min(res.value, 1.0f), -1.0f);
    return res;
}

unsigned midi_controller::get_button_count() const
{
    return control_buttons.size();
}

std::string midi_controller::get_button_name(unsigned i) const
{
    constexpr const char* const control_names[] = {
        "Damper",
        "Portamento",
        "Sustenuto",
        "Soft pedal"
    };
    if(i < sizeof(control_names)/sizeof(*control_names)) 
        return control_names[i];
    else return "Control " + std::to_string(i);
}

unsigned midi_controller::get_button_state(unsigned i) const
{
    return control_buttons[i];
}
