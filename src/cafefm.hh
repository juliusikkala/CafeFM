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
#ifndef CAFEFM_HH
#define CAFEFM_HH
#include <GL/glew.h>
#include "fm.hh"
#include "control_state.hh"
#include "instrument_state.hh"
#include "options.hh"
#include "bindings.hh"
#include "controller/controller.hh"
#include "controller/midi.hh"
#include "audio.hh"
#include "nuklear.hh"
#include "SDL.h"
#include "SDL_opengl.h"
#include <memory>
#include <map>

class cafefm
{
public:
    cafefm();
    ~cafefm();

    void load();
    void unload();
    void render();
    bool update(unsigned dt);

private:
    static constexpr unsigned CHANGE_NONE = 0;
    static constexpr unsigned CHANGE_REQUIRE_IMPORT = 1;
    static constexpr unsigned CHANGE_REQUIRE_RESET = 2;

    struct controller_data
    {
        std::unique_ptr<class controller> controller;
        int id;
        std::vector<bindings> presets;
        int selected_preset;
        bindings binds;
        bool active;
    };

    void handle_controller(
        controller_data* c, int axis_index, int button_index
    );

    void gui_keyboard_grab();
    void gui_controller_manager();
    void gui_draw_adsr(const envelope& adsr);
    unsigned gui_oscillator_type(oscillator::func& type, bool down = true);
    unsigned gui_modulation_mode();
    unsigned gui_adsr();
    unsigned gui_oscillator(
        oscillator& osc,
        unsigned index,
        bool& erase,
        unsigned partition,
        bool down,
        bool is_carrier,
        bool removable
    );
    void gui_instrument_editor();

    void gui_bind_action_template(bind& b);
    void gui_bind_action(bind& b);
    void gui_bind_modifiers(
        bind& b,
        bool allow_toggle,
        bool allow_cumulative,
        bool allow_threshold
    );
    void gui_bind_button(bind& b, bool discrete_only = false);
    void gui_bind_control_template(bind& b);
    // Return values:
    // 1 - move up
    // 0 - keep
    // -1 - move down
    // -2 - remove
    int gui_bind_control(bind& b, bool discrete_only = false);
    nk_color gui_bind_background_color(bind& b);
    int gui_bind(bind& b, unsigned index);
    void gui_bindings_editor();

    void gui_loop(unsigned loop_index);
    void gui_loops_editor();

    void gui_options_editor();

    void gui();

    void connect_controller(controller* c);
    void disconnect_controller(unsigned index);

    void select_controller(controller_data* c);
    void select_controller_preset(controller_data* c, unsigned index);
    void deactivate_controller(controller_data* c);
    void save_current_bindings(controller_data* c);
    void create_new_bindings(controller_data* c);
    void delete_bindings(const std::string& name);
    void update_all_bindings();
    void update_bindings_presets();

    void select_instrument(unsigned index);
    void save_current_instrument();
    void create_new_instrument();
    void delete_current_instrument();
    void update_all_instruments();

    void next_protip();

    void reset_fm(bool refresh_only = true);

    void apply_options(const options& new_opts);

    nk_context* ctx;
    SDL_Window* win;
    int ww, wh;
    SDL_GLContext gl_ctx;

    SDL_Surface* icon;

    struct nk_font* small_font;
    struct nk_font* medium_font;
    struct nk_font* huge_font;
    struct nk_image yellow_warn_img, gray_warn_img; 
    unsigned selected_tab;
    int selected_instrument_preset;
    bool bindings_delete_popup_open;
    bool instrument_delete_popup_open;
    int save_recording_state;
    // Used for assigning binds
    int latest_input_button, latest_input_axis;
    unsigned protip_index;

    midi_context midi;

    std::map<std::string, bindings> all_bindings;

    std::vector<std::unique_ptr<controller_data>> available_controllers;
    controller_data* selected_controller;
    int controller_id_counter;

    // Some controllers have special gui features (mouse, keyboard) for
    // grabbing/detaching them.
    bool keyboard_grabbed, mouse_grabbed;

    std::unique_ptr<fm_instrument> fm;
    std::unique_ptr<audio_output> output;

    float master_volume;
    control_state control;

    std::vector<instrument_state> all_instruments;
    instrument_state ins_state;

    options opts;
};

#endif
