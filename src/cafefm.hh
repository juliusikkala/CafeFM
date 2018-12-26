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
    void render();
    bool update(unsigned dt);

private:
    void gui();
    void reset_synth(
        uint64_t samplerate = 44100,
        oscillator_type carrier = OSC_SINE,
        const std::vector<dynamic_oscillator>& modulators = {}
    );

    nk_context* ctx;
    SDL_Window* win;
    SDL_GLContext gl_ctx;
    std::unique_ptr<basic_fm_synth> synth;
    std::unique_ptr<audio_output> output;
};

#endif
