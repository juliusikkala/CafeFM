#ifndef CAFEFM_HH
#define CAFEFM_HH
#include <GL/glew.h>
#include "fm.hh"
#include "control_state.hh"
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

    void gui_bind_control_template(bind& b);
    void gui_bind_control(bind& b, bool discrete_only = false);
    nk_color gui_bind_background_color(bind& b);
    void gui_key_bind(bind& b, unsigned index);
    void gui_instrument_editor();

    void gui();

    void select_controller(controller* c);
    void select_compatible_bindings(unsigned index);
    void save_current_bindings();
    void create_new_bindings();
    void delete_bindings(const std::string& name);

    void update_all_bindings();
    void update_compatible_bindings();

    void reset_synth(
        uint64_t samplerate = 44100,
        oscillator_type carrier = OSC_SINE,
        const std::vector<dynamic_oscillator>& modulators = {}
    );

    nk_context* ctx;
    SDL_Window* win;
    SDL_GLContext gl_ctx;

    struct nk_font* small_font;
    struct nk_font* medium_font;
    struct nk_font* huge_font;
    struct nk_image close_img, yellow_warn_img, gray_warn_img, red_warn_img,
        lock_img;
    unsigned selected_tab;
    int selected_preset;
    bool delete_popup_open;

    std::vector<std::unique_ptr<controller>> available_controllers;
    controller* selected_controller;

    // Some controllers have special gui features (mouse, keyboard) for
    // grabbing/detaching them.
    bool keyboard_grabbed, mouse_grabbed;

    std::unique_ptr<basic_fm_synth> synth;
    std::unique_ptr<audio_output> output;

    std::vector<dynamic_oscillator> modulators;
    float master_volume;
    control_state control;

    std::map<std::string, bindings> all_bindings;
    std::vector<bindings> compatible_bindings;
    bindings binds;
};

#endif
