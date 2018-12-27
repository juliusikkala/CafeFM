#include "cafefm.hh"
#include <stdexcept>
#include <string>

/* Significant parts related to GUI rendering copied from here (public domain):
 * https://github.com/vurtun/nuklear/blob/2891c6afbc5781b700cbad6f6d771f1f214f6b56/demo/sdl_opengl3/main.c
 */
#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024
#define CARRIER_HEIGHT 120
#define OSCILLATOR_HEIGHT 90
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

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
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
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

    int id = -1;
#define load_texture(name, path) \
    id = nk_sdl_create_texture_from_file(path, 0, 0); \
    if(id == -1) throw std::runtime_error("Failed to load image " path); \
    name = nk_image_id(id); \

    load_texture(close_img, "data/images/close.png");
    load_texture(warn_img, "data/images/warning.png");
}

void cafefm::unload()
{
    nk_sdl_destroy_texture(close_img.handle.id);
    nk_sdl_destroy_texture(warn_img.handle.id);
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

void cafefm::gui_draw_adsr(const envelope& adsr)
{
    struct nk_command_buffer *canvas = nk_window_get_canvas(ctx);
    struct nk_rect s = nk_window_get_content_region(ctx);
    struct nk_rect t = s;
    static constexpr int pad = 4;
    t.x += pad;
    t.y += pad;
    t.w -= 2*pad;
    t.h -= 2*pad;

    nk_layout_space_begin(ctx, NK_STATIC, s.h, 1);
    float total_length = adsr.attack_length + adsr.decay_length;

    if(adsr.sustain_volume_num != 0)
        total_length = (total_length + adsr.release_length)*2.0f;

    float attack_x = t.x+(adsr.attack_length/total_length)*t.w;
    float decay_x = attack_x+(adsr.decay_length/total_length)*t.w;
    float sustain_x = decay_x+((
            total_length - adsr.attack_length
            - adsr.decay_length
            - adsr.release_length
        )/total_length)*t.w;

    float sustain_y =
        t.h-(adsr.sustain_volume_num/(double)adsr.peak_volume_num)*t.h;

    const struct nk_color bg_color = nk_rgb(30, 30, 30);
    const struct nk_color line_color = nk_rgb(175, 175, 175);
    const struct nk_color border_color = nk_rgb(100, 100, 100);
    // Border & background
    nk_fill_rect(canvas, s, 4.0, bg_color);
    nk_stroke_rect(canvas, s, 4.0, 4.0f, border_color);

    // Attack
    nk_stroke_line(
        canvas, attack_x, s.y+s.h, attack_x, s.y, 2.0f, border_color
    );
    nk_stroke_line(canvas, t.x, t.y+t.h, attack_x, t.y, 2.0f, line_color);

    // Decay
    if(adsr.sustain_volume_num != 0)
    {
        nk_stroke_line(
            canvas, decay_x, s.y+s.h, decay_x, s.y, 2.0f, border_color
        );
    }
    nk_stroke_line(
        canvas, attack_x, t.y, decay_x, t.y+sustain_y, 2.0f, line_color
    );

    if(adsr.sustain_volume_num != 0)
    {
        // Sustain
        nk_stroke_line(
            canvas, sustain_x, s.y+s.h, sustain_x, s.y, 2.0f, border_color
        );
        nk_stroke_line(
            canvas, decay_x, t.y+sustain_y, sustain_x, t.y+sustain_y,
            2.0f, line_color
        );

        // Release
        nk_stroke_line(
            canvas, sustain_x, t.y+sustain_y, t.x+t.w, t.y+t.h, 2.0f, line_color
        );
    }
    nk_layout_space_end(ctx);
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

    nk_layout_row_dynamic(ctx, CARRIER_HEIGHT, 1);
    if(nk_group_begin(
        ctx,
        "Carrier Group",
        NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, CARRIER_HEIGHT-10);
        nk_layout_row_template_push_static(ctx, 200);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, 250);
        nk_layout_row_template_end(ctx);

        synth->get_volume();
        envelope adsr = synth->get_envelope();

        // Waveform type selection
        nk_style_set_font(ctx, &small_font->handle);
        if(nk_group_begin(ctx, "Carrier Waveform", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_dynamic(ctx, 30, 1);
            nk_label(ctx, "Waveform:", NK_TEXT_LEFT);
            mask |= gui_oscillator_type(type);
            nk_group_end(ctx);
        }

        // ADSR controls
        if(nk_group_begin(ctx, "Carrier ADSR Control", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_template_begin(ctx, 22);
            nk_layout_row_template_push_static(ctx, 110);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);
            nk_label(ctx, "Sustain volume:", NK_TEXT_LEFT);

            int sustain_vol= adsr.sustain_volume_num;
            nk_slider_int(ctx, 0, &sustain_vol, adsr.volume_denom, 1);
            adsr.sustain_volume_num = sustain_vol;

            static constexpr float expt = 4.0f;
            unsigned samplerate = output->get_samplerate();

            float attack = adsr.attack_length/(double)samplerate;
            nk_labelf(ctx, NK_TEXT_LEFT, "Attack: %.2fs", attack);
            attack = pow(attack, 1.0/expt);
            nk_slider_float(
                ctx, pow(0.001f, 1.0/expt), &attack, pow(4.0f, 1.0/expt), 0.01f
            );
            adsr.attack_length = pow(attack, expt)*samplerate;

            float decay = adsr.decay_length/(double)samplerate;
            nk_labelf(ctx, NK_TEXT_LEFT, "Decay: %.2fs", decay);
            decay = pow(decay, 1.0/expt);
            nk_slider_float(
                ctx, pow(0.001f, 1.0/expt), &decay, pow(4.0f, 1.0/expt), 0.01f
            );
            adsr.decay_length = pow(decay, expt)*samplerate;

            float release = adsr.release_length/(double)samplerate;
            nk_labelf(ctx, NK_TEXT_LEFT, "Release: %.2fs", release);
            release = pow(release, 1.0/expt);
            nk_slider_float(
                ctx, pow(0.001f, 1.0/expt), &release, pow(4.0f, 1.0/expt), 0.01f
            );
            adsr.release_length = pow(release, expt)*samplerate;

            nk_group_end(ctx);
        }

        // ADSR plot
        if(nk_group_begin(ctx, "Carrier ADSR Plot", NK_WINDOW_NO_SCROLLBAR))
        {
            gui_draw_adsr(adsr);
            nk_group_end(ctx);
        }
        synth->set_envelope(adsr);

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

    nk_layout_row_dynamic(ctx, OSCILLATOR_HEIGHT, 1);
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

        // Waveform type selection
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

        // Amplitude controls
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

        // Period controls
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

            uint64_t period_denom = 0, period_num = 0;
            osc.get_period(period_num, period_denom);
            int new_period_denom = period_denom;

            nk_labelf(
                ctx, NK_TEXT_LEFT, "%.2f", period_denom/(double)period_num
            );
            nk_slider_int(
                ctx, 1, &new_period_denom, period_num*8, period_num/16
            );
            if((uint64_t)new_period_denom != period_denom)
            {
                osc.set_period_fract(period_num, new_period_denom);
                mask |= CHANGE_REQUIRE_IMPORT;
            }

            nk_group_end(ctx);
        }
        
        // Remove button
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

void cafefm::gui_synth_editor()
{
    struct nk_rect s = nk_window_get_content_region(ctx);
    unsigned mask = CHANGE_NONE;
    oscillator_type carrier = synth->get_carrier_type();

    nk_style_set_font(ctx, &small_font->handle);
    // Render carrier & ADSR
    mask |= gui_carrier(carrier);

    std::vector<dynamic_oscillator> modulators;
    synth->export_modulators(modulators);

    // Render modulators
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

    // + -button for modulators
    nk_style_set_font(ctx, &huge_font->handle);
    nk_layout_row_dynamic(ctx, OSCILLATOR_HEIGHT, 1);
    if(modulators.size() <= 2 && nk_button_label(ctx, "+"))
    {
        modulators.push_back({OSC_SINE, 1.0, 0.5});
        mask |= CHANGE_REQUIRE_RESET;
    }

    // Various options at the bottom of the screen
    unsigned synth_control_height =
        s.h-CARRIER_HEIGHT-3*OSCILLATOR_HEIGHT-20;
    nk_style_set_font(ctx, &small_font->handle);
    nk_layout_space_begin(ctx, NK_STATIC, synth_control_height, 1);
    nk_layout_space_push(
        ctx, nk_layout_space_rect_to_local(ctx, nk_rect(
            s.x+4,s.y+s.h-synth_control_height,s.w-8,synth_control_height)));
    if(nk_group_begin(
        ctx, "Synth Control", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        float master_volume = synth->get_volume();
        int polyphony = synth->get_polyphony();
        float max_safe_volume = 1.0/synth->get_polyphony();

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 130);
        nk_layout_row_template_push_static(ctx, 400);
        nk_layout_row_template_push_static(ctx, 30);
        nk_layout_row_template_push_static(ctx, 130);
        nk_layout_row_template_end(ctx);

        nk_labelf(ctx, NK_TEXT_LEFT, "Master volume: %.2f", master_volume);
        nk_slider_float(ctx, 0, &master_volume, 1.0f, 0.01f);
        synth->set_volume(master_volume);

        if(master_volume > max_safe_volume)
        {
            nk_image(ctx, warn_img);
            nk_label(ctx, "Peaking is possible!", NK_TEXT_LEFT);
        }

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 130);
        nk_layout_row_template_push_static(ctx, 400);
        nk_layout_row_template_end(ctx);

        nk_labelf(ctx, NK_TEXT_LEFT, "Polyphony: %d", polyphony);
        int new_polyphony = polyphony;
        nk_slider_int(ctx, 1, &new_polyphony, 32, 1);
        if(new_polyphony != polyphony)
        {
            output->stop();
            synth->set_polyphony(new_polyphony);
            output->start();
        }

        nk_group_end(ctx);
    }
    nk_layout_space_end(ctx);

    // Do necessary updates
    if(mask & CHANGE_REQUIRE_RESET)
        reset_synth(44100, carrier, modulators);

    if(mask & CHANGE_REQUIRE_IMPORT)
        synth->import_modulators(modulators);
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
        gui_synth_editor();
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
        new_synth->set_volume(0.5);
    }
    synth.swap(new_synth);
    output.reset(new audio_output(*synth));
    output->start();
}
