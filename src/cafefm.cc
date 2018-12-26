#include "cafefm.hh"
#include <stdexcept>
#include <string>

/* Significant parts related to GUI rendering copied from here (public domain):
 * https://github.com/vurtun/nuklear/blob/2891c6afbc5781b700cbad6f6d771f1f214f6b56/demo/sdl_opengl3/main.c
 */
#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024

using namespace std::string_literals;

cafefm::cafefm()
: win(nullptr)
{

    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_FLAGS,
        SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG
    );
    SDL_GL_SetAttribute(
        SDL_GL_CONTEXT_PROFILE_MASK,
        SDL_GL_CONTEXT_PROFILE_CORE
    );
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    win = SDL_CreateWindow(
        "CafeFM",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        640,
        480,
        SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI
    );

    if(!win)
        throw std::runtime_error("Unable to open window: "s + SDL_GetError());

    gl_ctx = SDL_GL_CreateContext(win);
    if(!gl_ctx)
        throw std::runtime_error(
            "Unable to open GL context: "s + SDL_GetError()
        );

    SDL_GL_MakeCurrent(win, gl_ctx);

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if(err != GLEW_OK)
        throw std::runtime_error((const char*)glewGetErrorString(err));

    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    SDL_GL_SetSwapInterval(0);

    ctx = nk_sdl_init(win);
}

cafefm::~cafefm()
{
    nk_sdl_shutdown();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(win);

    if(output) output->stop();
}

void cafefm::load()
{
    reset_synth();

    struct nk_font_atlas* atlas;
    nk_sdl_font_stash_begin(&atlas);
    struct nk_font *montserrat = nk_font_atlas_add_from_file(atlas, "data/fonts/Montserrat/Montserrat-Medium.ttf", 23, 0);
    nk_sdl_font_stash_end();
    nk_style_set_font(ctx, &montserrat->handle);
}

void cafefm::render()
{
    gui();

    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.05,0.05,0.05,0);
    glClear(GL_COLOR_BUFFER_BIT);
    nk_sdl_render(NK_ANTI_ALIASING_OFF, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
    SDL_GL_SwapWindow(win);
}

bool cafefm::update(unsigned dt)
{
    bool quit = false;
    SDL_Event e;
    while(SDL_PollEvent(&e))
    {
        bool handled = false;
        switch(e.type)
        {
        case SDL_QUIT:
            handled = true;
            quit = true;
            break;
        default:
            break;
        }
        if(!handled) nk_sdl_handle_event(&e);
    }
    return !quit;
}

void cafefm::gui()
{
    int w, h;
    nk_input_end(ctx);

    SDL_GetWindowSize(win, &w, &h);
    if(nk_begin(ctx, "Instrument", nk_rect(0, 0, w, h), NK_WINDOW_NO_SCROLLBAR))
    {
        /* Render FM synth options */
        for(unsigned i = 0; i < 4; ++i)
        {
            nk_layout_row_dynamic(ctx, 90, 1);
            if(nk_button_label(ctx, "+"))
                printf("Naps naps %d\n", i);
        }
    }
    nk_end(ctx);

    nk_input_begin(ctx);
}

void cafefm::reset_synth(
    uint64_t samplerate,
    oscillator_type carrier,
    const std::vector<dynamic_oscillator>& modulators
){
    if(output) output->stop();
    output.reset(nullptr);

    std::unique_ptr<basic_fm_synth> new_synth(
        create_fm_synth(samplerate, carrier, modulators)
    );
    if(synth) new_synth->copy_state(*synth);
    else
    {
        new_synth->set_polyphony(16);
        new_synth->set_volume(0.25);
    }
    synth.swap(new_synth);
    output.reset(new audio_output(*synth));
    output->start();
}
