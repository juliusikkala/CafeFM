#ifndef CAFEFM_HH
#define CAFEFM_HH
#include <GL/glew.h>
#include "fm.hh"
#include "audio.hh"
#include "nuklear.hh"
#include "SDL.h"
#include "SDL_opengl.h"
#include <memory>

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

    void gui_draw_adsr(const envelope& adsr);
    unsigned gui_oscillator_type(oscillator_type& type);
    unsigned gui_carrier(oscillator_type& type);
    unsigned gui_modulator(
        dynamic_oscillator& osc,
        unsigned index,
        bool& erase
    );
    void gui_synth_editor();

    void gui();

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
    struct nk_image close_img, warn_img;
    unsigned selected_tab;

    std::unique_ptr<basic_fm_synth> synth;
    std::unique_ptr<audio_output> output;
};

#endif
