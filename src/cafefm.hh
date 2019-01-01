#ifndef CAFEFM_HH
#define CAFEFM_HH
#include <GL/glew.h>
#include "fm.hh"
#include "control_state.hh"
#include "synth_state.hh"
#include "bindings.hh"
#include "controller/controller.hh"
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

    void handle_controller(
        controller* c, int axis_1d_index, int axis_2d_index, int button_index
    );
    void detach_controller(unsigned index);
    void set_controller_grab(bool grab);

    void gui_keyboard_grab();
    void gui_controller_manager();
    void gui_draw_adsr(const envelope& adsr);
    unsigned gui_oscillator_type(oscillator_type& type);
    unsigned gui_carrier(oscillator_type& type);
    unsigned gui_modulator(
        dynamic_oscillator& osc,
        unsigned index,
        bool& erase
    );
    void gui_synth_editor();

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
    void gui_instrument_editor();

    void gui();

    void select_controller(controller* c);
    void select_compatible_bindings(unsigned index);
    void save_current_bindings();
    void create_new_bindings();
    void delete_bindings(const std::string& name);
    void update_all_bindings();
    void update_compatible_bindings();

    void select_synth(unsigned index);
    void save_current_synth();
    void create_new_synth();
    void delete_current_synth();
    void update_all_synths();

    void reset_synth(bool refresh_only = true);

    nk_context* ctx;
    SDL_Window* win;
    SDL_GLContext gl_ctx;

    struct nk_font* small_font;
    struct nk_font* medium_font;
    struct nk_font* huge_font;
    struct nk_image yellow_warn_img, gray_warn_img; 
    unsigned selected_tab;
    int selected_bindings_preset;
    int selected_synth_preset;
    bool bindings_delete_popup_open;
    bool synth_delete_popup_open;
    // Used for assigning binds
    int latest_input_button, latest_input_axis_1d, latest_input_axis_2d;

    std::vector<std::unique_ptr<controller>> available_controllers;
    controller* selected_controller;

    // Some controllers have special gui features (mouse, keyboard) for
    // grabbing/detaching them.
    bool keyboard_grabbed, mouse_grabbed;

    std::unique_ptr<basic_fm_synth> fm_synth;
    std::unique_ptr<audio_output> output;

    float master_volume;
    control_state control;

    uint64_t samplerate;
    std::vector<synth_state> all_synths;
    synth_state synth;

    std::map<std::string, bindings> all_bindings;
    std::vector<bindings> compatible_bindings;
    bindings binds;
};

#endif
