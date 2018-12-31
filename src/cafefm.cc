#include "cafefm.hh"
#include "controller/keyboard.hh"
#include "io.hh"
#include "helpers.hh"
#include <stdexcept>
#include <string>
#include <cstring>

/* Significant parts related to GUI rendering copied from here (public domain):
 * https://github.com/vurtun/nuklear/blob/2891c6afbc5781b700cbad6f6d771f1f214f6b56/demo/sdl_opengl3/main.c
 */
#define MAX_VERTEX_MEMORY 512 * 1024
#define MAX_ELEMENT_MEMORY 128 * 1024
#define CARRIER_HEIGHT 120
#define OSCILLATOR_HEIGHT 90
#define INSTRUMENT_HEADER_HEIGHT 110
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MAX_BINDING_NAME_LENGTH 128

using namespace std::string_literals;

namespace
{
    int button_label_active(
        struct nk_context* ctx,
        const char *title,
        bool active
    ){
        if(active) return nk_button_label(ctx, title);
        else
        {
            struct nk_style_button button;
            button = ctx->style.button;
            ctx->style.button.normal = nk_style_item_color(nk_rgb(40,40,40));
            ctx->style.button.hover = nk_style_item_color(nk_rgb(40,40,40));
            ctx->style.button.active = nk_style_item_color(nk_rgb(40,40,40));
            ctx->style.button.border_color = nk_rgb(60,60,60);
            ctx->style.button.text_background = nk_rgb(60,60,60);
            ctx->style.button.text_normal = nk_rgb(60,60,60);
            ctx->style.button.text_hover = nk_rgb(60,60,60);
            ctx->style.button.text_active = nk_rgb(60,60,60);
            nk_button_label(ctx, title);
            ctx->style.button = button;
            return 0;
        }
    }
};

cafefm::cafefm()
:   win(nullptr), delete_popup_open(false), selected_controller(nullptr),
    keyboard_grabbed(false), mouse_grabbed(false), master_volume(0.5)
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
    control.apply(*synth, master_volume, modulators);

    selected_tab = 1;
    selected_preset = -1;

    // Determine data directories
    fs::path font_dir("data/fonts");
    fs::path img_dir("data/images");
#ifdef DATA_DIRECTORY
    fs::path data_dir(DATA_DIRECTORY);
#ifdef NDEBUG
    if(fs::is_directory(data_dir/"data"))
#else
    if(!fs::is_directory(font_dir) || !fs::is_directory(img_dir))
#endif
    {
        font_dir = DATA_DIRECTORY/font_dir;
        img_dir = DATA_DIRECTORY/img_dir;
    }
#endif

    // Load fonts
    static const nk_rune nk_font_glyph_ranges[] = {
        0x0020, 0x024F,
        0x0370, 0x03FF,
        0x2200, 0x22FF,
        0x2C60, 0x2C7F,
        0x2600, 0x26FF,
        0x3000, 0x303F,
        0x3040, 0x309F,
        0x30A0, 0x30FF,
        0x4E00, 0x9FFF,
        0xFF00, 0xFFEF,
        0
    };
    struct nk_font_config config = nk_font_config(16);
    config.range = nk_font_glyph_ranges;

    struct nk_font_atlas* atlas;
    nk_sdl_font_stash_begin(&atlas);
    std::string font_file =
        (font_dir/"DejaVuSans/DejaVuSans.ttf").string();
    small_font = nk_font_atlas_add_from_file(
        atlas, font_file.c_str(), 16, &config
    );
    medium_font = nk_font_atlas_add_from_file(atlas, font_file.c_str(), 19, 0);
    huge_font = nk_font_atlas_add_from_file(atlas, font_file.c_str(), 23, 0);
    nk_sdl_font_stash_end();

    // Load textures
    int id = -1;
    std::string img_path;
#define load_texture(name, path) \
    img_path = (img_dir/path).string(); \
    id = nk_sdl_create_texture_from_file(img_path.c_str(), 0, 0); \
    if(id == -1) throw std::runtime_error("Failed to load image " + img_path); \
    name = nk_image_id(id); \

    load_texture(close_img, "close.png");
    load_texture(yellow_warn_img, "yellow_warning.png");
    load_texture(gray_warn_img, "gray_warning.png");
    load_texture(red_warn_img, "red_warning.png");
    load_texture(lock_img, "lock.png");

    // Setup controllers
    available_controllers.emplace_back(new keyboard());
    select_controller(available_controllers[0].get());
}

void cafefm::unload()
{
    available_controllers.clear();
    all_bindings.clear();
    compatible_bindings.clear();

    selected_controller = nullptr;
    delete_popup_open = false;

    nk_sdl_destroy_texture(close_img.handle.id);
    nk_sdl_destroy_texture(yellow_warn_img.handle.id);
    nk_sdl_destroy_texture(red_warn_img.handle.id);
    nk_sdl_destroy_texture(gray_warn_img.handle.id);
    nk_sdl_destroy_texture(lock_img.handle.id);
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

    auto cb = [this](
        controller* c, int axis_1d_index, int axis_2d_index, int button_index
    ){
        handle_controller(c, axis_1d_index, axis_2d_index, button_index);
    };

    for(unsigned i = 0; i < available_controllers.size(); ++i)
    {
        auto& c = available_controllers[i];
        auto fn = c.get() == selected_controller ?
            cb : controller::change_callback();
        if(!c->poll(fn)) detach_controller(i);
    }

    SDL_Event e;
    while(SDL_PollEvent(&e))
    {
        bool handled = false;
        bool is_keyboard_event = false;
        bool is_mouse_event = false;

        switch(e.type)
        {
        case SDL_QUIT:
            handled = true;
            quit = true;
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        case SDL_TEXTINPUT:
        case SDL_TEXTEDITING:
            is_keyboard_event = true;
            break;
        case SDL_MOUSEMOTION:
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
        case SDL_MOUSEWHEEL:
            is_mouse_event = true;
            break;
        default:
            break;
        }
        
        if(
            (is_keyboard_event && keyboard_grabbed) ||
            (is_mouse_event && mouse_grabbed)
        ) handled = true;

        for(unsigned i = 0; i < available_controllers.size(); ++i)
        {
            auto& c = available_controllers[i];
            auto fn = c.get() == selected_controller ?
                cb : controller::change_callback();
            if(!c->handle_event(e, fn)) detach_controller(i);
        }

        if(!handled) nk_sdl_handle_event(&e);
    }

    control.apply(*synth, master_volume, modulators);
    return !quit;
}

void cafefm::handle_controller(
    controller* c, int axis_1d_index, int axis_2d_index, int button_index
){
    std::string type = c->get_type_name();
    if(
        c != selected_controller ||
        (type == "Keyboard" && !keyboard_grabbed) ||
        (type == "Mouse" && !mouse_grabbed)
    ) return;

    latest_input_button = button_index;
    latest_input_axis_1d = axis_1d_index;
    latest_input_axis_2d = axis_2d_index;

    binds.act(control, c, axis_1d_index, axis_2d_index, button_index);
}

void cafefm::detach_controller(unsigned index)
{
    controller* c = available_controllers[index].get();
    if(selected_controller == c) selected_controller = nullptr;
    available_controllers.erase(available_controllers.begin() + index);
}

void cafefm::set_controller_grab(bool grab)
{
    if(selected_controller)
    {
        std::string type = selected_controller->get_type_name();
        if(type == "Keyboard") keyboard_grabbed = grab;
    }
}

void cafefm::gui_keyboard_grab()
{
    nk_style_set_font(ctx, &medium_font->handle);
    if(keyboard_grabbed)
    {
        struct nk_style_item button_color = ctx->style.button.normal;
        ctx->style.button.normal = ctx->style.button.active;
        if(nk_button_label(ctx, "Detach keyboard"))
            keyboard_grabbed = false;
        ctx->style.button.normal = button_color;
    }
    else
    {
        if(nk_button_label(ctx, "Grab keyboard"))
            keyboard_grabbed = true;
    }
}

void cafefm::gui_controller_manager()
{
    if(selected_controller)
    {
        std::string type = selected_controller->get_type_name();
        if(type == "Keyboard") gui_keyboard_grab();
    }
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

    // + button for modulators
    nk_style_set_font(ctx, &huge_font->handle);
    nk_layout_row_dynamic(ctx, OSCILLATOR_HEIGHT, 1);
    if(modulators.size() <= 2 && nk_button_symbol(ctx, NK_SYMBOL_PLUS))
    {
        modulators.push_back({OSC_SINE, 1.0, 0.5});
        mask |= CHANGE_REQUIRE_RESET;
    }

    // Various options at the bottom of the screen
    unsigned synth_control_height =
        s.h-CARRIER_HEIGHT-3*OSCILLATOR_HEIGHT-50;
    nk_style_set_font(ctx, &small_font->handle);
    nk_layout_space_begin(ctx, NK_STATIC, synth_control_height, 1);
    nk_layout_space_push(
        ctx, nk_layout_space_rect_to_local(ctx, nk_rect(
            s.x+4,s.y+s.h-synth_control_height,s.w-8,synth_control_height-5)));
    if(nk_group_begin(
        ctx, "Synth Control", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        int polyphony = synth->get_polyphony();
        float max_safe_volume = 1.0/synth->get_polyphony();

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 140);
        nk_layout_row_template_push_static(ctx, 400);
        nk_layout_row_template_push_static(ctx, 30);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);

        nk_labelf(ctx, NK_TEXT_LEFT, "Master volume: %.2f", master_volume);
        nk_slider_float(ctx, 0, &master_volume, 1.0f, 0.01f);

        if(master_volume > max_safe_volume)
        {
            nk_image(ctx, gray_warn_img);
            nk_label(ctx, "Peaking is possible!", NK_TEXT_LEFT);
        }

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 140);
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


        nk_layout_row_static(ctx, 60, 150, 1);
        gui_controller_manager();

        nk_group_end(ctx);
    }
    nk_layout_space_end(ctx);

    // Do necessary updates
    if(mask & CHANGE_REQUIRE_RESET)
    {
        reset_synth(44100, carrier, modulators);
        control.apply(*synth, master_volume, modulators);
    }
}

void cafefm::gui_bind_button(bind& b, bool discrete_only)
{
    if(!selected_controller) return;

    std::string label = "Unknown";
    switch(b.control)
    {
    case bind::UNBOUND:
        label = "Assign";
        break;
    case bind::BUTTON_PRESS:
    case bind::BUTTON_TOGGLE:
        if(
            b.button.index >= 0 &&
            b.button.index < (int)selected_controller->get_button_count()
        ) label = selected_controller->get_button_name(b.button.index);
        break;
    case bind::AXIS_1D_CONTINUOUS:
    case bind::AXIS_1D_RELATIVE:
    case bind::AXIS_1D_THRESHOLD:
    case bind::AXIS_1D_THRESHOLD_TOGGLE:
        if(
            b.axis_1d.index >= 0 &&
            b.axis_1d.index < (int)selected_controller->get_axis_1d_count()
        ) label = selected_controller->get_axis_1d_name(b.axis_1d.index);
        break;
    }

    if(!b.wait_assign && nk_button_label(ctx, label.c_str()))
    {
        b.wait_assign = true;
        latest_input_button = -1;
        latest_input_axis_1d = -1;
        latest_input_axis_2d = -1;
        set_controller_grab(true);
    }
    else if(b.wait_assign)
    {
        if(latest_input_button >= 0)
        {
            b.control = bind::BUTTON_PRESS;
            b.button.index = latest_input_button;
            b.button.active_state = 1;
            b.wait_assign = false;
        }
        if(latest_input_axis_1d >= 0)
        {
            b.control = discrete_only ?
                bind::AXIS_1D_THRESHOLD : bind::AXIS_1D_CONTINUOUS;
            b.axis_1d.index = latest_input_axis_1d;
            b.axis_1d.invert = false;
            b.axis_1d.threshold = 0.5;
            b.wait_assign = false;
        }
        else
        {
            struct nk_style_item button_color = ctx->style.button.normal;
            ctx->style.button.normal = ctx->style.button.active;

            if(nk_button_label(ctx, "Waiting"))
                b.wait_assign = false;

            ctx->style.button.normal = button_color;
        }
    }
}

void cafefm::gui_bind_control_template(bind& b)
{
    switch(b.control)
    {
    case bind::UNBOUND:
        break;
    case bind::BUTTON_PRESS:
    case bind::BUTTON_TOGGLE:
        nk_layout_row_template_push_static(ctx, 80);
        break;
    case bind::AXIS_1D_CONTINUOUS:
    case bind::AXIS_1D_RELATIVE:
        break;
    case bind::AXIS_1D_THRESHOLD:
    case bind::AXIS_1D_THRESHOLD_TOGGLE:
        nk_layout_row_template_push_static(ctx, 80);
        break;
    }
    nk_layout_row_template_push_static(ctx, 80);
    nk_layout_row_template_push_static(ctx, 25);
    nk_layout_row_template_push_static(ctx, 25);
    nk_layout_row_template_push_static(ctx, 25);
}

int cafefm::gui_bind_control(bind& b, bool discrete_only)
{
    switch(b.control)
    {
    case bind::BUTTON_PRESS:
        if(nk_check_label(ctx, "Toggle", 1) == 0)
            b.control = bind::BUTTON_TOGGLE;
        break;
    case bind::BUTTON_TOGGLE:
        if(nk_check_label(ctx, "Toggle", 0) == 1)
            b.control = bind::BUTTON_PRESS;
        break;
    case bind::AXIS_1D_THRESHOLD:
        if(nk_check_label(ctx, "Toggle", 1) == 0)
            b.control = bind::AXIS_1D_THRESHOLD_TOGGLE;
        break;
    case bind::AXIS_1D_THRESHOLD_TOGGLE:
        if(nk_check_label(ctx, "Toggle", 0) == 1)
            b.control = bind::AXIS_1D_THRESHOLD;
        break;
    default:
        break;
    }

    gui_bind_button(b, discrete_only);

    int ret = 0;
    if(nk_button_symbol(ctx, NK_SYMBOL_TRIANGLE_UP)) ret = 1;
    if(nk_button_symbol(ctx, NK_SYMBOL_TRIANGLE_DOWN)) ret = -1;
    if(nk_button_symbol(ctx, NK_SYMBOL_X)) ret = -2;
    return ret;
}

nk_color cafefm::gui_bind_background_color(bind& b)
{
    nk_color bg = ctx->style.window.background;
    if(!selected_controller) return bg;

    nk_color active = nk_rgb(30,30,30);
    double value = fabs(b.get_value(control, selected_controller));
    value = std::min(1.0, value);
    bg.r = round(lerp(bg.r, active.r, value));
    bg.g = round(lerp(bg.g, active.g, value));
    bg.b = round(lerp(bg.b, active.b, value));
    return bg;
}

int cafefm::gui_key_bind(bind& b, unsigned index)
{
    static constexpr int min_semitone = -45;
    static const std::vector<std::string> note_list =
        generate_note_list(min_semitone, min_semitone + 96);

    std::string title = "Key Bind " + std::to_string(index);

    nk_color bg = gui_bind_background_color(b);

    struct nk_style *s = &ctx->style;
    nk_style_push_style_item(ctx, &s->window.fixed_background, nk_style_item_color(bg));

    int ret = 0;
    struct nk_rect empty_space;
    if(nk_group_begin(
        ctx, title.c_str(), NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, 25);
        nk_layout_row_template_push_static(ctx, 80);
        nk_layout_row_template_push_dynamic(ctx);
        gui_bind_control_template(b);
        nk_layout_row_template_end(ctx);

        std::string note_name = generate_semitone_name(b.key_semitone);

        bool was_open = ctx->current->popup.win;
        if(nk_combo_begin_label(ctx, note_name.c_str(), nk_vec2(80, 200)))
        {
            nk_layout_row_dynamic(ctx, 30, 1);
            unsigned match_i = 0;
            for(unsigned i = 0; i < note_list.size(); ++i)
            {
                if(note_list[i] == note_name) match_i = i;
                if(nk_combo_item_label(ctx, note_list[i].c_str(), NK_TEXT_LEFT))
                    b.key_semitone = min_semitone + i;
            }

            if(!was_open)
            {
                struct nk_window *win = ctx->current;
                win->scrollbar.y = 34*match_i;
            }

            nk_combo_end(ctx);
        }
        
        nk_widget(&empty_space, ctx);

        ret = gui_bind_control(b, true);
        nk_group_end(ctx);
    }

    nk_style_pop_style_item(ctx);
    return ret;
}

int cafefm::gui_freq_bind(bind& b, unsigned index)
{
    std::string title = "Freq Bind " + std::to_string(index);

    nk_color bg = gui_bind_background_color(b);

    struct nk_style *s = &ctx->style;
    nk_style_push_style_item(ctx, &s->window.fixed_background, nk_style_item_color(bg));

    int ret = 0;
    struct nk_rect empty_space;
    if(nk_group_begin(
        ctx, title.c_str(), NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, 25);
        nk_layout_row_template_push_static(ctx, 140);
        nk_layout_row_template_push_dynamic(ctx);
        gui_bind_control_template(b);
        nk_layout_row_template_end(ctx);

        nk_property_double(
            ctx, "Offset:", -72.0f, &b.frequency.max_expt, 72.0f, 0.5f, 0.5f
        );

        nk_widget(&empty_space, ctx);

        ret = gui_bind_control(b, true);
        nk_group_end(ctx);
    }

    nk_style_pop_style_item(ctx);
    return ret;
}

int cafefm::gui_volume_bind(bind& b, unsigned index)
{
    std::string title = "Volume Bind " + std::to_string(index);

    nk_color bg = gui_bind_background_color(b);

    struct nk_style *s = &ctx->style;
    nk_style_push_style_item(ctx, &s->window.fixed_background, nk_style_item_color(bg));

    int ret = 0;
    struct nk_rect empty_space;
    if(nk_group_begin(
        ctx, title.c_str(), NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, 25);
        nk_layout_row_template_push_static(ctx, 105);
        nk_layout_row_template_push_static(ctx, 105);
        nk_layout_row_template_push_dynamic(ctx);
        gui_bind_control_template(b);
        nk_layout_row_template_end(ctx);

        // Stupid hacks to fix nuklear's rounding and decimals.
        constexpr double eps = 0.001;
        constexpr double step = 0.01;
        double new_min = nk_propertyd(
            ctx, "Min:", eps, b.volume.min_mul+eps, 2.0+eps, 0.05, step
        )-eps; new_min = round(new_min/step)*step;
        if(new_min != b.volume.min_mul)
        {
            printf("%f\n", new_min);
            b.volume.min_mul = new_min;
            if(new_min > b.volume.max_mul)
                b.volume.max_mul = new_min;
        }
        double new_max = nk_propertyd(
            ctx, "Max:", eps, b.volume.max_mul+eps, 2.0+eps, 0.05, step
        )-eps; new_max = round(new_max/step)*step;
        if(new_max != b.volume.max_mul)
        {
            b.volume.max_mul = new_max;
            if(new_max < b.volume.min_mul)
                b.volume.min_mul = new_max;
        }

        nk_widget(&empty_space, ctx);

        ret = gui_bind_control(b, true);
        nk_group_end(ctx);
    }

    nk_style_pop_style_item(ctx);
    return ret;
}

void cafefm::gui_instrument_editor()
{
    nk_style_set_font(ctx, &small_font->handle);
    nk_layout_row_dynamic(ctx, INSTRUMENT_HEADER_HEIGHT, 1);

    // Controller & bindings selection.
    if(nk_group_begin(
        ctx, "Controller Group", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 80);
        nk_layout_row_template_push_static(ctx, 400);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);

        // Controller picker
        nk_label(ctx, "Controller:", NK_TEXT_LEFT);

        std::vector<std::string> label_strings;
        std::vector<const char*> label_cstrings;
        unsigned selected_index = 0;
        for(unsigned i = 0; i < available_controllers.size(); ++i)
        {
            auto& c = available_controllers[i];
            if(c.get() == selected_controller) selected_index = i;

            std::string name = c->get_device_name();
            label_strings.push_back(name);
            label_cstrings.push_back(name.c_str());
        }

        unsigned new_selected_index = nk_combo(
            ctx, label_cstrings.data(), label_cstrings.size(),
            selected_index, 25, nk_vec2(400, 200)
        );
        if(new_selected_index != selected_index)
            select_controller(available_controllers[selected_index].get());

        // Show manager (Grab Keyboard, etc.)
        gui_controller_manager();
        nk_style_set_font(ctx, &small_font->handle);

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 80);
        nk_layout_row_template_push_static(ctx, 400);
        nk_layout_row_template_push_static(ctx, 30);
        nk_layout_row_template_push_static(ctx, 230);
        nk_layout_row_template_end(ctx);

        // Preset picker
        nk_label(ctx, "Preset:", NK_TEXT_LEFT);

        std::string combo_label = "(None)";
        if(selected_preset >= 0)
            combo_label = compatible_bindings[selected_preset].get_name();

        if(nk_combo_begin_label(ctx, combo_label.c_str(), nk_vec2(400,200)))
        {
            int new_selected_preset = -1;
            nk_layout_row_dynamic(ctx, 25, 1);
            for(unsigned i = 0; i < compatible_bindings.size(); ++i)
            {
                const auto& b = compatible_bindings[i];
                unsigned comp = b.rate_compatibility(selected_controller);
                if(comp == 0)
                {
                    if(nk_combo_item_label(
                        ctx, b.get_name().c_str(), NK_TEXT_ALIGN_LEFT
                    )) new_selected_preset = i;
                }
                else if(comp == 1)
                {
                    if(nk_combo_item_image_label(
                        ctx, gray_warn_img, b.get_name().c_str(),
                        NK_TEXT_ALIGN_LEFT
                    )) new_selected_preset = i;
                }
                else if(comp == 2)
                {
                    if(nk_combo_item_image_label(
                        ctx, yellow_warn_img, b.get_name().c_str(),
                        NK_TEXT_ALIGN_LEFT
                    )) new_selected_preset = i;
                }
            }

            if(new_selected_preset != -1)
                select_compatible_bindings(new_selected_preset);
            nk_combo_end(ctx);
        }

        // Warn user for suboptimal bindings
        switch(binds.rate_compatibility(selected_controller))
        {
        case 1:
            nk_image(ctx, gray_warn_img);
            nk_label(
                ctx, "This preset is for a different device", NK_TEXT_LEFT
            );
            break;
        case 2:
            nk_image(ctx, yellow_warn_img);
            nk_label(
                ctx, "This preset may be incompatible", NK_TEXT_LEFT
            );
            break;
        default:
            break;
        }

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 484);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);

        // Name editor
        std::string current_name_str = binds.get_name();
        char current_name[MAX_BINDING_NAME_LENGTH+1] = {0};
        int current_name_len = current_name_str.size();
        strncpy(
            current_name, current_name_str.c_str(), MAX_BINDING_NAME_LENGTH
        );
        nk_edit_string(
            ctx, NK_EDIT_SIMPLE, current_name, &current_name_len,
            MAX_BINDING_NAME_LENGTH, nk_filter_default
        );
        binds.set_name(std::string(current_name, current_name_len));

        // Save, New and Delete buttons
        auto name_match = all_bindings.find(binds.get_name());
        bool can_save = false;
        bool can_delete = false;
        if(name_match == all_bindings.end()) can_save = true;
        else
        {
            bool locked = name_match->second.is_write_locked();
            can_save = !locked;
            can_delete = !locked;
        }

        if(button_label_active(ctx, "Save", can_save))
            save_current_bindings();

        if(nk_button_label(ctx, "New"))
            create_new_bindings();

        if(button_label_active(ctx, "Delete", can_delete))
            delete_popup_open = true;

        // Handle delete popup
        if(delete_popup_open)
        {
            struct nk_rect s = {0, 100, 300, 136};
            s.x = WINDOW_WIDTH/2-s.w/2;
            if(nk_popup_begin(
                ctx, NK_POPUP_STATIC, "Delete?",
                NK_WINDOW_BORDER|NK_WINDOW_TITLE, s
            )){
                std::string warning_text =
                    "Are you sure you want to delete preset \"";
                warning_text += binds.get_name() + "\"?";
                nk_layout_row_dynamic(ctx, 50, 1);
                nk_label_wrap(ctx, warning_text.c_str());
                nk_layout_row_dynamic(ctx, 30, 2);
                if (nk_button_label(ctx, "Delete"))
                {
                    delete_popup_open = false;
                    delete_bindings(binds.get_name());
                    nk_popup_close(ctx);
                }
                if (nk_button_label(ctx, "Cancel"))
                {
                    delete_popup_open = false;
                    nk_popup_close(ctx);
                }
                nk_popup_end(ctx);
            }
            else delete_popup_open = false;
        }

        nk_group_end(ctx);
    }

    nk_layout_row_dynamic(
        ctx,
        WINDOW_HEIGHT-INSTRUMENT_HEADER_HEIGHT-43,
        1
    );

    // Actual bindings section
    if(nk_group_begin(ctx, "Bindings Group", NK_WINDOW_BORDER))
    {
#define section(title, act, func) \
        if(nk_tree_push(ctx, NK_TREE_TAB, title, NK_MINIMIZED)) \
        { \
            nk_layout_row_dynamic(ctx, 35, 1); \
            int changed_index = -1; \
            int movement = 0; \
            for(unsigned i = 0; i < binds.bind_count(); ++i) \
            { \
                auto& b = binds.get_bind(i); \
                if(b.action != act) continue; \
                int ret = func (b, i); \
                if(ret != 0) \
                { \
                    changed_index = i; \
                    movement = ret; \
                } \
            } \
            if(movement) \
                binds.move_bind(changed_index, movement, control, true); \
            nk_style_set_font(ctx, &huge_font->handle); \
            if(nk_button_symbol(ctx, NK_SYMBOL_PLUS)) \
                binds.create_new_bind(act); \
            nk_style_set_font(ctx, &small_font->handle); \
            nk_tree_pop(ctx); \
        }

        section("Keys", bind::KEY, gui_key_bind)
        section("Pitch", bind::FREQUENCY_EXPT, gui_freq_bind)
        section("Volume", bind::VOLUME_MUL, gui_volume_bind)

        if(nk_tree_push(ctx, NK_TREE_TAB, "Modulator period", NK_MINIMIZED))
        {
            nk_layout_row_dynamic(ctx, 35, 1);

            nk_style_set_font(ctx, &huge_font->handle);
            if(nk_button_symbol(ctx, NK_SYMBOL_PLUS))
                printf("Should add a period control, I guess.\n");
            nk_style_set_font(ctx, &small_font->handle);

            nk_tree_pop(ctx);
        }
        if(nk_tree_push(ctx, NK_TREE_TAB, "Modulator amplitude", NK_MINIMIZED))
        {
            nk_layout_row_dynamic(ctx, 35, 1);

            nk_style_set_font(ctx, &huge_font->handle);
            if(nk_button_symbol(ctx, NK_SYMBOL_PLUS))
                printf("Should add an amplitude control, I guess.\n");
            nk_style_set_font(ctx, &small_font->handle);

            nk_tree_pop(ctx);
        }
        nk_group_end(ctx);
    }
}

void cafefm::gui()
{
    int w, h;
    nk_input_end(ctx);

    SDL_GetWindowSize(win, &w, &h);

    const char* tabs[] = {
        "Synth",
        "Instrument",
        "Options"
    };
    unsigned tab_count = sizeof(tabs)/sizeof(*tabs);

    nk_style_set_font(ctx, &medium_font->handle);
    if(nk_begin(
        ctx,
        "CafeFM",
        nk_rect(0, 0, w, h),
        NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BACKGROUND
    )){
        nk_style_push_vec2(ctx, &ctx->style.window.spacing, nk_vec2(0,0));
        nk_style_push_float(ctx, &ctx->style.button.rounding, 0);
        nk_layout_row_begin(ctx, NK_STATIC, 30, 3);
        for(unsigned i = 0; i < tab_count; ++i)
        {
            const struct nk_user_font *f = &medium_font->handle;
            float text_width = f->width(
                f->userdata, f->height, tabs[i], strlen(tabs[i])
            );
            float widget_width = 10 + text_width + 3 * ctx->style.button.padding.x;
            nk_layout_row_push(ctx, widget_width);
            if(selected_tab == i)
            {
                struct nk_style_item button_color = ctx->style.button.normal;
                ctx->style.button.normal = ctx->style.button.active;
                selected_tab = nk_button_label(ctx, tabs[i]) ? i: selected_tab;
                ctx->style.button.normal = button_color;
            }
            else
            {
                selected_tab = nk_button_label(ctx, tabs[i]) ? i: selected_tab;
            }
        }
        nk_layout_row_end(ctx);
        nk_style_pop_float(ctx);
        nk_style_pop_vec2(ctx);

        nk_style_set_font(ctx, &huge_font->handle);

        switch(selected_tab)
        {
        case 0:
            gui_synth_editor();
            break;
        case 1:
            gui_instrument_editor();
            break;
        default:
            break;
        }
    }
    nk_end(ctx);

    nk_input_begin(ctx);
}

void cafefm::select_controller(controller* c)
{
    selected_controller = c;

    synth->release_all_voices();
    control.reset();
    update_compatible_bindings();
    if(compatible_bindings.size() >= 1)
    {
        selected_preset = 0;
        binds = compatible_bindings[0];
    }
    else selected_preset = -1;
}

void cafefm::select_compatible_bindings(unsigned index)
{
    synth->release_all_voices();
    control.reset();
    selected_preset = index;
    binds = compatible_bindings[index];
}

void cafefm::save_current_bindings()
{
    if(selected_controller)
        binds.set_target_device(selected_controller);

    write_bindings(binds);
    update_compatible_bindings();

    for(unsigned i = 0; i < compatible_bindings.size(); ++i)
    {
        if(compatible_bindings[i].get_name() == binds.get_name())
        {
            selected_preset = i;
            break;
        }
    }
}

void cafefm::create_new_bindings()
{
    synth->release_all_voices();
    control.reset();
    selected_preset = -1;
    binds.clear();

    if(selected_controller)
        binds.set_target_device(selected_controller);

    std::string name = "New bindings";
    if(all_bindings.count(name) == 0) binds.set_name(name);
    else
    {
        unsigned i = 1;
        name += " #";
        std::string modified_name;
        do
        {
            modified_name = name + std::to_string(i++);
        }
        while(all_bindings.count(modified_name) != 0);
        binds.set_name(modified_name);
    }
}

void cafefm::delete_bindings(const std::string& name)
{
    auto it = all_bindings.find(name);
    if(it == all_bindings.end()) return;

    remove_bindings(it->second);
    update_compatible_bindings();
    select_compatible_bindings(selected_preset);
}

void cafefm::update_all_bindings()
{
    all_bindings.clear();
    auto all_bindings_vector = load_all_bindings();
    for(const bindings& b: all_bindings_vector)
        all_bindings.emplace(b.get_name(), b);
}

void cafefm::update_compatible_bindings()
{
    update_all_bindings();

    compatible_bindings.clear();

    if(!selected_controller) return;
    for(auto& pair: all_bindings)
    {
        unsigned comp = pair.second.rate_compatibility(selected_controller);
        if(comp <= 2) compatible_bindings.emplace_back(pair.second);
    }

    auto compare = [](const bindings& a, const bindings& b){
        return a.get_name() < b.get_name();
    };

    std::sort(compatible_bindings.begin(), compatible_bindings.end(), compare);

    if(compatible_bindings.size() == 0) selected_preset = -1;
    if(selected_preset > (int)compatible_bindings.size())
        selected_preset = compatible_bindings.size() - 1;
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
