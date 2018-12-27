#include "cafefm.hh"
#include <stdexcept>
#include <string>

/* Significant parts related to GUI rendering copied from here (public domain):
 * https://github.com/vurtun/nuklear/blob/2891c6afbc5781b700cbad6f6d771f1f214f6b56/demo/sdl_opengl3/main.c
 */
#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024
#define OSCILLATOR_HEIGHT 90

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
        800,
        600,
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
    unload();
    if(output) output->stop();

    nk_sdl_shutdown();
    SDL_GL_DeleteContext(gl_ctx);
    SDL_DestroyWindow(win);
}

void cafefm::load()
{
    reset_synth();

    struct nk_font_atlas* atlas;
    nk_sdl_font_stash_begin(&atlas);
    small_font = nk_font_atlas_add_from_file(atlas, "data/fonts/Montserrat/Montserrat-Medium.ttf", 16, 0);
    huge_font = nk_font_atlas_add_from_file(atlas, "data/fonts/Montserrat/Montserrat-Medium.ttf", 23, 0);
    nk_sdl_font_stash_end();

    int id = nk_sdl_create_texture_from_file("data/images/close.png", 0, 0);
    if(id == -1) throw std::runtime_error("Failed to load image");
    close_img = nk_image_id(id);
}

void cafefm::unload()
{
    nk_sdl_destroy_texture(close_img.handle.id);
}

void cafefm::render()
{
    gui();

    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
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
        case SDL_KEYDOWN:
            if(!e.key.repeat && e.key.keysym.sym == SDLK_SPACE)
                synth->press_voice(0, 0);
            break;
        case SDL_KEYUP:
            if(e.key.keysym.sym == SDLK_SPACE)
                synth->release_voice(0);
            break;
        default:
            break;
        }
        if(!handled) nk_sdl_handle_event(&e);
    }
    return !quit;
}

unsigned cafefm::gui_oscillator_type(oscillator_type& type)
{
    static const char* oscillator_labels[] = {
        "Sine",
        "Square",
        "Triangle",
        "Saw"
    };

    oscillator_type old_type = type;
    type = (oscillator_type)nk_combo(
        ctx,
        oscillator_labels,
        sizeof(oscillator_labels)/sizeof(const char*),
        old_type,
        25,
        nk_vec2(200, 200)
    );
    return old_type != type ? CHANGE_REQUIRE_RESET : CHANGE_NONE;
}

unsigned cafefm::gui_carrier(oscillator_type& type)
{
    unsigned mask = CHANGE_NONE;

    nk_layout_row_dynamic(ctx, OSCILLATOR_HEIGHT, 1);
    if(nk_group_begin(
        ctx,
        "Carrier Group",
        NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, OSCILLATOR_HEIGHT-10);
        nk_layout_row_template_push_static(ctx, 200);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);

        nk_style_set_font(ctx, &small_font->handle);
        if(nk_group_begin(ctx, "Carrier Waveform", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Waveform:", NK_TEXT_LEFT);
            mask |= gui_oscillator_type(type);
            nk_group_end(ctx);
        }
        nk_label(ctx, "Carrier", NK_TEXT_LEFT);
        nk_group_end(ctx);
    }

    return mask;
}

unsigned cafefm::gui_modulator(
    dynamic_oscillator& osc,
    unsigned index,
    bool& erase
){
    static std::string name_table[] = {
        "First modulator",
        "Second modulator",
        "Third modulator"
    };
    unsigned mask = CHANGE_NONE;

    if(nk_group_begin(
        ctx,
        (name_table[index]+" Group").c_str(),
        NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, OSCILLATOR_HEIGHT);
        nk_layout_row_template_push_static(ctx, 200);
        nk_layout_row_template_push_static(ctx, 200);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, 48);
        nk_layout_row_template_end(ctx);

        nk_style_set_font(ctx, &small_font->handle);
        if(nk_group_begin(
            ctx,
            (name_table[index]+" Waveform").c_str(),
            NK_WINDOW_NO_SCROLLBAR
        )){
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Waveform:", NK_TEXT_LEFT);
            oscillator_type type = osc.get_type();
            mask |= gui_oscillator_type(type);
            osc.set_type(type);

            nk_group_end(ctx);
        }

        if(nk_group_begin(
            ctx,
            (name_table[index]+" Amplitude").c_str(),
            NK_WINDOW_NO_SCROLLBAR
        )){
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Amplitude:", NK_TEXT_LEFT);
            nk_layout_row_template_begin(ctx, 30);
            nk_layout_row_template_push_static(ctx, 30);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);

            float old_amplitude = osc.get_amplitude();
            float amplitude = old_amplitude;
            nk_labelf(ctx, NK_TEXT_LEFT, "%.2f", old_amplitude);
            nk_slider_float(ctx, 0, &amplitude, 2.0f, 0.05f);
            if(amplitude != old_amplitude)
            {
                osc.set_amplitude(amplitude);
                mask |= CHANGE_REQUIRE_IMPORT;
            }

            nk_group_end(ctx);
        }

        if(nk_group_begin(
            ctx,
            (name_table[index]+" Period").c_str(),
            NK_WINDOW_NO_SCROLLBAR
        )){
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Period:", NK_TEXT_LEFT);
            nk_layout_row_template_begin(ctx, 30);
            nk_layout_row_template_push_static(ctx, 30);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);

            float old_period = osc.get_period();
            float period = old_period;
            nk_labelf(ctx, NK_TEXT_LEFT, "%.2f", old_period);
            nk_slider_float(ctx, 0, &period, 8.0f, 0.05f);
            if(period != old_period)
            {
                osc.set_period(period);
                mask |= CHANGE_REQUIRE_IMPORT;
            }

            nk_group_end(ctx);
        }
        
        if(nk_group_begin(
            ctx,
            (name_table[index]+" Remove").c_str(),
            NK_WINDOW_NO_SCROLLBAR
        )){
            nk_layout_row_static(ctx, 32, 32, 1);
            nk_style_set_font(ctx, &huge_font->handle);
            if(nk_button_image(ctx, close_img))
            {
                erase = true;
                mask |= CHANGE_REQUIRE_RESET;
            }
            nk_group_end(ctx);
        }
        nk_group_end(ctx);
    }

    return mask;
}

void cafefm::gui()
{
    int w, h;
    nk_input_end(ctx);

    SDL_GetWindowSize(win, &w, &h);
    nk_style_set_font(ctx, &huge_font->handle);

    if(nk_begin(
        ctx,
        "Instrument",
        nk_rect(0, 0, w, h),
        NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BACKGROUND
    )){
        unsigned mask = CHANGE_NONE;
        oscillator_type carrier = synth->get_carrier_type();

        nk_style_set_font(ctx, &small_font->handle);
        /* Render FM synth options */
        mask |= gui_carrier(carrier);

        std::vector<dynamic_oscillator> modulators;
        synth->export_modulators(modulators);

        for(unsigned i = 0; i < modulators.size(); ++i)
        {
            bool erase = false;
            mask |= gui_modulator(modulators[i], i, erase);
            if(erase)
            {
                modulators.erase(modulators.begin() + i);
                --i;
            }
        }

        nk_style_set_font(ctx, &huge_font->handle);
        nk_layout_row_dynamic(ctx, OSCILLATOR_HEIGHT, 1);
        if(modulators.size() <= 2 && nk_button_label(ctx, "+"))
        {
            modulators.push_back({OSC_SINE, 1.0, 0.5});
            mask |= CHANGE_REQUIRE_RESET;
        }

        if(mask & CHANGE_REQUIRE_RESET)
            reset_synth(44100, carrier, modulators);

        if(mask & CHANGE_REQUIRE_IMPORT)
            synth->import_modulators(modulators);
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
