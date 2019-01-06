#include "cafefm.hh"
#include "controller/keyboard.hh"
#include "controller/gamecontroller.hh"
#include "controller/joystick.hh"
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
#define INSTRUMENT_HEADER_HEIGHT 110
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MAX_BINDING_NAME_LENGTH 128
#define MAX_INSTRUMENT_NAME_LENGTH 128

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
            ctx->style.button.normal = nk_style_item_color(nk_rgb(40,34,31));
            ctx->style.button.hover = nk_style_item_color(nk_rgb(40,34,31));
            ctx->style.button.active = nk_style_item_color(nk_rgb(40,34,31));
            ctx->style.button.border_color = nk_rgb(60,51,47);
            ctx->style.button.text_background = nk_rgb(60,51,47);
            ctx->style.button.text_normal = nk_rgb(60,51,47);
            ctx->style.button.text_hover = nk_rgb(60,51,47);
            ctx->style.button.text_active = nk_rgb(60,51,47);
            nk_button_label(ctx, title);
            ctx->style.button = button;
            return 0;
        }
    }

    // Stupid hack to fix nuklear's rounding and decimals.
    double fixed_propertyd(
        struct nk_context *ctx, const char *name, double min,
        double val, double max, double step, double inc_per_pixel,
        double eps = 0.00001
    ){
        double mstep = std::min(step, inc_per_pixel);
        double ret = nk_propertyd(
            ctx, name, min+eps, val+eps, max+eps, step, inc_per_pixel
        )-eps;
        return round(ret/mstep)*mstep;
    }

    void axis_widget(
        struct nk_context* ctx,
        double input_value,
        bool is_signed,
        double* threshold
    ){
        struct nk_rect bounds;
        nk_widget_layout_states state;
        state = nk_widget(&bounds, ctx);
        if (!state) return;

        struct nk_command_buffer *out = &ctx->current->buffer;
        struct nk_style_progress* style = &ctx->style.progress;
        struct nk_style_item* background = &style->normal;

        nk_input* in = threshold ? &ctx->input : nullptr;

        struct nk_rect click_area = bounds;
        struct nk_rect cursor = bounds;

        // Handle input
        if(
            in &&
            nk_input_has_mouse_click_down_in_rect(
                in, NK_BUTTON_LEFT, click_area, nk_true
            )
        ){
            double new_threshold = (in->mouse.pos.x - cursor.x) / cursor.w;
            new_threshold = std::min(new_threshold, 1.0);
            new_threshold = std::max(new_threshold, 0.0);
            *threshold = is_signed ?  new_threshold * 2.0 - 1.0 : new_threshold;
        }

        double normalized_input =
            is_signed ? input_value*0.5 + 0.5 : input_value;
        cursor.w = normalized_input * cursor.w;

        // Draw

        struct nk_color line_color = nk_rgb(240, 0, 0);
        struct nk_color bar_color = style->cursor_normal.data.color;

        if(threshold && *threshold < input_value) bar_color = nk_rgb(175,0,0);

        // Background
        nk_fill_rect(out, bounds, style->rounding, background->data.color);
        nk_stroke_rect(
            out, bounds, style->rounding, style->border, style->border_color
        );

        // Bar
        nk_fill_rect(out, cursor, style->rounding, bar_color);
        nk_stroke_rect(
            out, cursor, style->rounding, style->border, style->border_color
        );

        // Threshold marker
        if(threshold)
        {
            double threshold_x = is_signed ?  *threshold*0.5 + 0.5 : *threshold;
            threshold_x = threshold_x * bounds.w + cursor.x;

            nk_stroke_line(
                out,
                threshold_x, cursor.y,
                threshold_x, cursor.y+cursor.h,
                2.0f,
                line_color
            );
        }

    }
};

cafefm::cafefm()
:   win(nullptr), bindings_delete_popup_open(false),
    instrument_delete_popup_open(false), selected_controller(nullptr),
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
    // Setup GUI state
    selected_tab = 0;
    selected_bindings_preset = -1;
    selected_instrument_preset = -1;

    // Determine data directories
    fs::path font_dir("data/fonts");
    fs::path img_dir("data/images");
    fs::path icon_dir("data/icon");
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
        icon_dir = DATA_DIRECTORY/icon_dir;
    }
#endif

    // Load icon first, to display it ASAP.
    icon = IMG_Load((icon_dir/"128.png").string().c_str());
    if(icon) SDL_SetWindowIcon(win, icon);

    // Load fonts
    static const nk_rune nk_font_glyph_ranges[] = {
        0x0020, 0x024F,
        0x0300, 0x03FF,
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

    // Setup theme
    struct nk_color fg = nk_rgba(175, 150, 130, 255);
    struct nk_color fg_hover = nk_rgba(205, 180, 160, 255);
    struct nk_color fg_active = nk_rgba(226, 178, 139, 255);
    struct nk_color table[NK_COLOR_COUNT];

    table[NK_COLOR_TEXT] = nk_rgba(195,190,185,255);
    table[NK_COLOR_WINDOW] = nk_rgba(45, 38, 35, 255);
    table[NK_COLOR_HEADER] = nk_rgba(40, 34, 31, 255);
    table[NK_COLOR_BORDER] = nk_rgba(65, 55, 51, 255);
    table[NK_COLOR_BUTTON] = nk_rgba(50, 42, 39, 255);
    table[NK_COLOR_BUTTON_HOVER] = nk_rgba(40, 34, 31, 255);
    table[NK_COLOR_BUTTON_ACTIVE] = nk_rgba(35, 30, 27, 255);
    table[NK_COLOR_TOGGLE] = nk_rgba(100,84,78,255);
    table[NK_COLOR_TOGGLE_HOVER] = nk_rgba(120,101,93,255);
    table[NK_COLOR_TOGGLE_CURSOR] = nk_rgba(45, 38, 35, 255);
    table[NK_COLOR_SELECT] = nk_rgba(45, 38, 35, 255);
    table[NK_COLOR_SELECT_ACTIVE] = nk_rgba(35, 30, 27, 255);
    table[NK_COLOR_SLIDER] = nk_rgba(38, 32, 30, 255);
    table[NK_COLOR_SLIDER_CURSOR] = fg;
    table[NK_COLOR_SLIDER_CURSOR_HOVER] = fg_hover;
    table[NK_COLOR_SLIDER_CURSOR_ACTIVE] = fg_active;
    table[NK_COLOR_PROPERTY] = nk_rgba(38, 32, 30, 255);
    table[NK_COLOR_EDIT] = nk_rgba(38, 32, 30, 255);
    table[NK_COLOR_EDIT_CURSOR] = nk_rgba(175,175,175,255);
    table[NK_COLOR_COMBO] = nk_rgba(45, 38, 35, 255);
    table[NK_COLOR_CHART] = nk_rgba(120,101,93,255);
    table[NK_COLOR_CHART_COLOR] = nk_rgba(45, 38, 35, 255);
    table[NK_COLOR_CHART_COLOR_HIGHLIGHT] = nk_rgba(255, 0,  0, 255);
    table[NK_COLOR_SCROLLBAR] = nk_rgba(38, 32, 30, 255);
    table[NK_COLOR_SCROLLBAR_CURSOR] = fg;
    table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = fg_hover;
    table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = fg_active;
    table[NK_COLOR_TAB_HEADER] = nk_rgba(40, 34, 31, 255);
    nk_style_from_table(ctx, table);

    // Load textures
    int id = -1;
    std::string img_path;
#define load_texture(name, path) \
    img_path = (img_dir/path).string(); \
    id = nk_sdl_create_texture_from_file(img_path.c_str(), 0, 0); \
    if(id == -1) throw std::runtime_error("Failed to load image " + img_path); \
    name = nk_image_id(id); \

    load_texture(yellow_warn_img, "yellow_warning.png");
    load_texture(gray_warn_img, "gray_warning.png");

    // Load instrument presets
    load_options(opts);

    update_all_instruments();
    create_new_instrument();

    // Setup controllers
    available_controllers.emplace_back(new keyboard());
    select_controller(available_controllers[0].get());
}

void cafefm::unload()
{
    selected_controller = nullptr;

    available_controllers.clear();
    all_bindings.clear();
    compatible_bindings.clear();

    bindings_delete_popup_open = false;
    instrument_delete_popup_open = false;

    nk_sdl_destroy_texture(yellow_warn_img.handle.id);
    nk_sdl_destroy_texture(gray_warn_img.handle.id);

    SDL_FreeSurface(icon);
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
        case SDL_JOYDEVICEADDED:
            if(SDL_IsGameController(e.jdevice.which))
            {
                available_controllers.emplace_back(
                    new gamecontroller(e.jdevice.which)
                );
            }
            else
            {
                available_controllers.emplace_back(
                    new joystick(e.jdevice.which)
                );
            }
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

    control.update(binds, dt);
    control.apply(*fm, master_volume, ins_state);
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
    // Make sure small inputs don't cause bind assignment
    if(
        axis_1d_index >= 0 &&
        fabs(c->get_axis_1d_state(axis_1d_index).value) > 0.5
    ) latest_input_axis_1d = axis_1d_index;

    latest_input_axis_2d = axis_2d_index;

    binds.act(control, c, axis_1d_index, axis_2d_index, button_index);
}

void cafefm::detach_controller(unsigned index)
{
    controller* c = available_controllers[index].get();
    available_controllers.erase(available_controllers.begin() + index);
    if(selected_controller == c)
    {
        selected_controller = nullptr;
        if(available_controllers.size() > 0)
            select_controller(available_controllers[0].get());
    }
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

    const struct nk_color bg_color = nk_rgb(38, 32, 30);
    const struct nk_color line_color = nk_rgb(175, 150, 130);
    const struct nk_color border_color = nk_rgb(100, 84, 78);
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

unsigned cafefm::gui_oscillator_type(oscillator::func& type, bool down)
{
    static const char* oscillator_labels[] = {
        "Sine",
        "Square",
        "Triangle",
        "Saw"
    };

    oscillator::func old_type = type;
    type = (oscillator::func)nk_combo(
        ctx,
        oscillator_labels,
        sizeof(oscillator_labels)/sizeof(const char*),
        old_type,
        20,
        nk_vec2(180, down?200:-200)
    );
    return old_type != type ? CHANGE_REQUIRE_RESET : CHANGE_NONE;
}

unsigned cafefm::gui_carrier(oscillator::func& type)
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

            int sustain_vol = ins_state.adsr.sustain_volume_num;
            nk_slider_int(ctx, 0, &sustain_vol, ins_state.adsr.volume_denom, 1);
            ins_state.adsr.sustain_volume_num = sustain_vol;

            static constexpr float expt = 4.0f;

            float attack = ins_state.adsr.attack_length/(double)opts.samplerate;
            nk_labelf(ctx, NK_TEXT_LEFT, "Attack: %.2fs", attack);
            attack = pow(attack, 1.0/expt);
            nk_slider_float(
                ctx, pow(0.001f, 1.0/expt), &attack, pow(4.0f, 1.0/expt), 0.01f
            );
            ins_state.adsr.attack_length = pow(attack, expt)*opts.samplerate;

            float decay = ins_state.adsr.decay_length/(double)opts.samplerate;
            nk_labelf(ctx, NK_TEXT_LEFT, "Decay: %.2fs", decay);
            decay = pow(decay, 1.0/expt);
            nk_slider_float(
                ctx, pow(0.001f, 1.0/expt), &decay, pow(4.0f, 1.0/expt), 0.01f
            );
            ins_state.adsr.decay_length = pow(decay, expt)*opts.samplerate;

            float release = ins_state.adsr.release_length/(double)opts.samplerate;
            nk_labelf(ctx, NK_TEXT_LEFT, "Release: %.2fs", release);
            release = pow(release, 1.0/expt);
            nk_slider_float(
                ctx, pow(0.001f, 1.0/expt), &release, pow(4.0f, 1.0/expt), 0.01f
            );
            ins_state.adsr.release_length = pow(release, expt)*opts.samplerate;

            nk_group_end(ctx);
        }

        // ADSR plot
        if(nk_group_begin(ctx, "Carrier ADSR Plot", NK_WINDOW_NO_SCROLLBAR))
        {
            gui_draw_adsr(ins_state.adsr);
            nk_group_end(ctx);
        }

        nk_group_end(ctx);
    }

    return mask;
}

unsigned cafefm::gui_modulator(
    oscillator& osc,
    unsigned index,
    bool& erase
){
    unsigned mask = CHANGE_NONE;

    nk_style_set_font(ctx, &small_font->handle);
    if(nk_group_begin(
        ctx,
        ("Modulator " + std::to_string(index)).c_str(),
        NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|NK_WINDOW_TITLE
    )){
        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 100);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, 30);
        nk_layout_row_template_end(ctx);

        // Waveform type selection
        nk_style_set_font(ctx, &small_font->handle);
        oscillator::func type = osc.get_type();
        mask |= gui_oscillator_type(
            type,
            index + 2 < ins_state.synth.get_modulator_count()
        );
        osc.set_type(type);

        // Amplitude controls
        float old_amplitude = osc.get_amplitude();
        float amplitude = fixed_propertyd(
            ctx, "#Amplitude:", 0.0, old_amplitude, 16.0, 0.01, 0.01
        );
        if(amplitude != old_amplitude)
        {
            osc.set_amplitude(amplitude);
            mask |= CHANGE_REQUIRE_IMPORT;
        }

        // Period controls
        uint64_t period_denom = 0, period_num = 0;
        osc.get_period(period_num, period_denom);
        double period = period_denom/(double)period_num;

        period = fixed_propertyd(
            ctx, "#Period:", 0.0, period, 1024.0, 0.01, 0.01
        );
        uint64_t new_period_denom = round(period*period_num);

        if(new_period_denom != period_denom)
        {
            osc.set_period_fract(period_num, new_period_denom);
            mask |= CHANGE_REQUIRE_IMPORT;
        }

        // Phase controls
        double phase = osc.get_phase_constant_double();
        double new_phase = fixed_propertyd(
            ctx, "#Phase:", 0.0, phase, 1.0, 0.01, 0.01
        );
        if(fabs(new_phase-phase)>1e-8)
        {
            osc.set_phase_constant(new_phase);
            mask |= CHANGE_REQUIRE_IMPORT;
        }
        
        // Remove button
        nk_style_set_font(ctx, &huge_font->handle);
        if(nk_button_symbol(ctx, NK_SYMBOL_X))
        {
            erase = true;
            mask |= CHANGE_REQUIRE_RESET;
        }
        nk_group_end(ctx);
    }

    return mask;
}

void cafefm::gui_instrument_editor()
{
    unsigned mask = CHANGE_NONE;

    nk_layout_row_dynamic(ctx, 155, 1);

    nk_style_set_font(ctx, &small_font->handle);

    // Save, etc. controls at the top
    if(nk_group_begin(
        ctx, "Instrument Control", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        float max_safe_volume = 1.0/ins_state.polyphony;

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 80);
        nk_layout_row_template_push_static(ctx, 400);
        nk_layout_row_template_push_static(ctx, 90);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);

        // Preset picker
        nk_label(ctx, "Preset:", NK_TEXT_LEFT);
        std::string combo_label = "(None)";
        if(selected_instrument_preset >= 0)
            combo_label = all_instruments[selected_instrument_preset].name;

        if(nk_combo_begin_label(ctx, combo_label.c_str(), nk_vec2(400,200)))
        {
            int new_selected_preset = -1;
            for(unsigned i = 0; i < all_instruments.size(); ++i)
            {
                const auto& s = all_instruments[i];
                nk_layout_row_dynamic(ctx, 25, 1);
                if(nk_combo_item_label(
                    ctx, s.name.c_str(), NK_TEXT_ALIGN_LEFT
                )) new_selected_preset = i;
            }

            if(new_selected_preset != -1)
                select_instrument(new_selected_preset);
            nk_combo_end(ctx);
        }

        if(nk_button_label(ctx, "Reset"))
        {
            fm->release_all_voices();
            control.reset();
        }

        gui_controller_manager();
        nk_style_set_font(ctx, &small_font->handle);

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 484);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);

        // Name editor
        char current_name[MAX_INSTRUMENT_NAME_LENGTH+1] = {0};
        int current_name_len = ins_state.name.size();
        strncpy(
            current_name,
            ins_state.name.c_str(),
            MAX_INSTRUMENT_NAME_LENGTH
        );
        nk_edit_string(
            ctx, NK_EDIT_SIMPLE, current_name, &current_name_len,
            MAX_INSTRUMENT_NAME_LENGTH, nk_filter_default
        );
        ins_state.name = std::string(current_name, current_name_len);

        // Save, New and Delete buttons
        auto name_match = all_instruments.end();
        for(
            auto it = all_instruments.begin();
            it != all_instruments.end();
            ++it
        ){
            if(it->name == ins_state.name) name_match = it;
        }
        
        bool can_save = false;
        bool can_delete = false;
        if(name_match == all_instruments.end()) can_save = true;
        else
        {
            can_save = !name_match->write_lock;
            can_delete = !name_match->write_lock;
        }

        if(button_label_active(ctx, "Save", can_save))
            save_current_instrument();

        if(nk_button_label(ctx, "New"))
            create_new_instrument();

        if(button_label_active(ctx, "Delete", can_delete))
            instrument_delete_popup_open = true;

        // Handle delete popup
        if(instrument_delete_popup_open)
        {
            struct nk_rect s = {0, 100, 300, 136};
            s.x = WINDOW_WIDTH/2-s.w/2;
            if(nk_popup_begin(
                ctx, NK_POPUP_STATIC, "Delete?",
                NK_WINDOW_BORDER|NK_WINDOW_TITLE, s
            )){
                std::string warning_text =
                    "Are you sure you want to delete preset \"";
                warning_text += ins_state.name + "\"?";
                nk_layout_row_dynamic(ctx, 50, 1);
                nk_label_wrap(ctx, warning_text.c_str());
                nk_layout_row_dynamic(ctx, 30, 2);
                if (nk_button_label(ctx, "Delete"))
                {
                    instrument_delete_popup_open = false;
                    delete_current_instrument();
                    nk_popup_close(ctx);
                }
                if (nk_button_label(ctx, "Cancel"))
                {
                    instrument_delete_popup_open = false;
                    nk_popup_close(ctx);
                }
                nk_popup_end(ctx);
            }
            else instrument_delete_popup_open = false;
        }

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

        nk_labelf(ctx, NK_TEXT_LEFT, "Polyphony: %d", ins_state.polyphony);
        int new_polyphony = ins_state.polyphony;
        nk_slider_int(ctx, 1, &new_polyphony, 32, 1);
        if((unsigned)new_polyphony != ins_state.polyphony)
        {
            output->stop();
            ins_state.polyphony = new_polyphony;
            fm->set_polyphony(ins_state.polyphony);
            output->start();
        }

        nk_group_end(ctx);
    }

    nk_style_set_font(ctx, &small_font->handle);

    // Render carrier & ADSR
    oscillator::func carrier = ins_state.synth.get_carrier_type();
    mask |= gui_carrier(carrier);
    ins_state.synth.set_carrier_type(carrier);

    nk_layout_row_dynamic(ctx, 280, 1);
    if(nk_group_begin(ctx, "Synth Control", NK_WINDOW_BORDER))
    {

        nk_layout_row_dynamic(ctx, 73, 1);
        // Render modulators
        for(unsigned i = 0; i < ins_state.synth.get_modulator_count(); ++i)
        {
            bool erase = false;
            mask |= gui_modulator(ins_state.synth.get_modulator(i), i, erase);
            if(erase)
            {
                ins_state.synth.erase_modulator(i);
                --i;
            }
        }

        // + button for modulators
        nk_style_set_font(ctx, &huge_font->handle);
        if(nk_button_symbol(ctx, NK_SYMBOL_PLUS))
        {
            unsigned i = ins_state.synth.add_modulator(
                {oscillator::SINE, 1.0, 0.5}
            );
            if(i > 0)
            {
                ins_state.synth.get_modulator(i-1).get_modulators().push_back(i);
            }
            else
            {
                ins_state.synth.get_carrier_modulators().push_back(i);
            }
            mask |= CHANGE_REQUIRE_RESET;
        }

        nk_group_end(ctx);
    }

    // Do necessary updates
    if(mask & CHANGE_REQUIRE_RESET)
    {
        ins_state.synth.finish_changes();
        reset_fm();
        control.apply(*fm, master_volume, ins_state);
    }
}

void cafefm::gui_bind_action_template(bind& b)
{
    switch(b.action)
    {
    case bind::KEY:
        nk_layout_row_template_push_static(ctx, 80);
        break;
    case bind::FREQUENCY_EXPT:
        nk_layout_row_template_push_static(ctx, 150);
        break;
    case bind::VOLUME_MUL:
        nk_layout_row_template_push_static(ctx, 150);
        break;
    case bind::PERIOD_EXPT:
        nk_layout_row_template_push_static(ctx, 150);
        nk_layout_row_template_push_static(ctx, 150);
        break;
    case bind::AMPLITUDE_MUL:
        nk_layout_row_template_push_static(ctx, 150);
        nk_layout_row_template_push_static(ctx, 150);
        break;
    case bind::ENVELOPE_ADJUST:
        nk_layout_row_template_push_static(ctx, 90);
        nk_layout_row_template_push_static(ctx, 150);
        break;
    }
}

void cafefm::gui_bind_action(bind& b)
{
    static constexpr int min_semitone = -45;
    static const std::vector<std::string> note_list =
        generate_note_list(min_semitone, min_semitone + 96);

    static const char* envelope_index_names[] = {
        "Attack", "Decay", "Sustain", "Release"
    };

    switch(b.action)
    {
    case bind::KEY:
        {
            std::string note_name = generate_semitone_name(b.key_semitone);

            bool was_open = ctx->current->popup.win;
            if(nk_combo_begin_label(ctx, note_name.c_str(), nk_vec2(80, 200)))
            {
                nk_layout_row_dynamic(ctx, 30, 1);
                unsigned match_i = 0;
                for(unsigned i = 0; i < note_list.size(); ++i)
                {
                    if(note_list[i] == note_name) match_i = i;
                    if(nk_combo_item_label(
                        ctx, note_list[i].c_str(), NK_TEXT_LEFT
                    )) b.key_semitone = min_semitone + i;
                }

                if(!was_open)
                {
                    struct nk_window *win = ctx->current;
                    if(win) win->scrollbar.y = 34*match_i;
                }

                nk_combo_end(ctx);
            }
        }
        break;
    case bind::FREQUENCY_EXPT:
        nk_property_double(
            ctx, "#Offset:", -72.0f, &b.frequency.max_expt, 72.0f, 0.5f, 0.5f
        );
        break;
    case bind::VOLUME_MUL:
        b.volume.max_mul = fixed_propertyd(
            ctx, "#Multiplier:", 0.0, b.volume.max_mul, 2.0, 0.05, 0.01
        );
        break;
    case bind::PERIOD_EXPT:
        nk_property_int(
            ctx, "#Modulator:", 0, (int*)&b.period.modulator_index, 128, 1, 1
        );
        nk_property_double(
            ctx, "#Offset:", -36.0f, &b.period.max_expt, 36.0f, 0.5f, 0.5f
        );
        break;
    case bind::AMPLITUDE_MUL:
        nk_property_int(
            ctx, "#Modulator:", 0, (int*)&b.amplitude.modulator_index, 128, 1, 1
        );
        b.amplitude.max_mul = fixed_propertyd(
            ctx, "#Multiplier:", 0.0, b.amplitude.max_mul, 8.0, 0.05, 0.01
        );
        break;
    case bind::ENVELOPE_ADJUST:
        nk_combobox(
            ctx, envelope_index_names,
            sizeof(envelope_index_names)/sizeof(*envelope_index_names),
            (int*)&b.envelope.which, 20, nk_vec2(80, 80)
        );
        if(b.envelope.which == 2)
        {
            if(b.envelope.max_mul > 2.0) b.envelope.max_mul = 2.0;
            b.envelope.max_mul = fixed_propertyd(
                ctx, "#Multiplier:", 0.0, b.envelope.max_mul, 2.0, 0.05, 0.01
            );
        }
        else nk_property_double(
            ctx, "#Multiplier:", 0, &b.envelope.max_mul, 128.0, 0.5, 0.25
        );
        break;
    }
}

void cafefm::gui_bind_modifiers(
    bind& b, bool allow_toggle, bool allow_cumulative, bool allow_threshold
){
    if(b.control == bind::UNBOUND) return;

    bool allow_invert = false;
    bool allow_stacking = allow_cumulative;

    if(
        b.control == bind::AXIS_1D_CONTINUOUS ||
        b.control == bind::AXIS_1D_THRESHOLD
    ){
        allow_invert = true;
    }
    else allow_threshold = false;
    if(b.control == bind::AXIS_1D_CONTINUOUS)
        allow_stacking = false;

    bool multiple =
        ((int)allow_toggle +
        (int)allow_cumulative +
        (int)allow_stacking +
        (int)allow_threshold +
        (int)allow_invert) > 1;

    if(multiple)
    {
        if(nk_combo_begin_label(ctx, "Modifiers", nk_vec2(130,200)))
            nk_layout_row_dynamic(ctx, 25, 1);
        else return;
    }

    if(allow_toggle)
        b.toggle = !nk_check_label(ctx, "Toggle", !b.toggle);

    if(allow_cumulative)
        b.cumulative = !nk_check_label(ctx, "Cumulative", !b.cumulative);

    if(allow_threshold)
    {
        bool has_threshold = b.control == bind::AXIS_1D_THRESHOLD;
        has_threshold = !nk_check_label(ctx, "Threshold", !has_threshold);
        if(has_threshold) b.control = bind::AXIS_1D_THRESHOLD;
        else b.control = bind::AXIS_1D_CONTINUOUS;
    }

    if(allow_stacking)
    {
        b.stacking = !nk_check_label(ctx, "Stacking", !b.stacking);
        if(b.stacking)
        {
            b.toggle = false;
            b.cumulative = false;
        }
    }

    if(allow_invert)
        b.axis_1d.invert = !nk_check_label(ctx, "Invert", !b.axis_1d.invert);

    if(multiple) nk_combo_end(ctx);
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
        if(
            b.button.index >= 0 &&
            b.button.index < (int)selected_controller->get_button_count()
        ) label = selected_controller->get_button_name(b.button.index);
        break;
    case bind::AXIS_1D_CONTINUOUS:
    case bind::AXIS_1D_THRESHOLD:
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
        nk_layout_row_template_push_static(ctx, 100);
        break;
    case bind::AXIS_1D_CONTINUOUS:
        nk_layout_row_template_push_static(ctx, 80);
        nk_layout_row_template_push_static(ctx, 100);
        break;
    case bind::AXIS_1D_THRESHOLD:
        nk_layout_row_template_push_static(ctx, 80);
        nk_layout_row_template_push_static(ctx, 100);
        break;
    }
    nk_layout_row_template_push_static(ctx, 100);
    nk_layout_row_template_push_static(ctx, 25);
    nk_layout_row_template_push_static(ctx, 25);
    nk_layout_row_template_push_static(ctx, 25);
}

int cafefm::gui_bind_control(bind& b, bool discrete_only)
{
    if(
        b.control == bind::AXIS_1D_CONTINUOUS ||
        b.control == bind::AXIS_1D_THRESHOLD
    ){
        bool is_signed;
        double input_value = b.input_value(selected_controller, &is_signed);
        axis_widget(
            ctx,
            input_value,
            is_signed,
            b.control == bind::AXIS_1D_CONTINUOUS ?
                nullptr : &b.axis_1d.threshold
        );
    }

    gui_bind_modifiers(
        b,
        b.control != bind::AXIS_1D_CONTINUOUS,
        b.action != bind::KEY,
        b.action != bind::KEY
    );

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

    nk_color active = nk_rgb(30,25,23);
    double value = fabs(b.get_value(control, selected_controller));
    value = std::min(1.0, value);
    bg.r = round(lerp(bg.r, active.r, value));
    bg.g = round(lerp(bg.g, active.g, value));
    bg.b = round(lerp(bg.b, active.b, value));
    return bg;
}

int cafefm::gui_bind(bind& b, unsigned index)
{
    std::string title = "Bind " + std::to_string(index);

    nk_color bg = gui_bind_background_color(b);

    struct nk_style *s = &ctx->style;
    nk_style_push_style_item(
        ctx,
        &s->window.fixed_background,
        nk_style_item_color(bg)
    );

    int ret = 0;
    struct nk_rect empty_space;
    if(nk_group_begin(
        ctx, title.c_str(), NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, 25);
        gui_bind_action_template(b);
        nk_layout_row_template_push_dynamic(ctx);
        gui_bind_control_template(b);
        nk_layout_row_template_end(ctx);

        gui_bind_action(b);

        nk_widget(&empty_space, ctx);

        ret = gui_bind_control(b, true);
        nk_group_end(ctx);
    }

    nk_style_pop_style_item(ctx);
    return ret;
}

void cafefm::gui_bindings_editor()
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
        nk_layout_row_template_push_static(ctx, 90);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);

        // Controller picker
        nk_label(ctx, "Controller:", NK_TEXT_LEFT);

        std::vector<std::string> label_strings;
        unsigned selected_index = 0;
        for(unsigned i = 0; i < available_controllers.size(); ++i)
        {
            auto& c = available_controllers[i];
            if(c.get() == selected_controller) selected_index = i;

            std::string name = c->get_device_name();
            label_strings.push_back(name);
        }

        std::vector<const char*> label_cstrings;
        for(std::string& str: label_strings)
            label_cstrings.push_back(str.c_str());

        unsigned new_selected_index = nk_combo(
            ctx, label_cstrings.data(), label_cstrings.size(),
            selected_index, 25, nk_vec2(400, 200)
        );
        if(new_selected_index != selected_index)
            select_controller(available_controllers[new_selected_index].get());

        if(nk_button_label(ctx, "Reset"))
        {
            fm->release_all_voices();
            control.reset();
        }
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
        if(selected_bindings_preset >= 0)
            combo_label = compatible_bindings[
                selected_bindings_preset
            ].get_name();

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
            bindings_delete_popup_open = true;

        // Handle delete popup
        if(bindings_delete_popup_open)
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
                    bindings_delete_popup_open = false;
                    delete_bindings(binds.get_name());
                    nk_popup_close(ctx);
                }
                if (nk_button_label(ctx, "Cancel"))
                {
                    bindings_delete_popup_open = false;
                    nk_popup_close(ctx);
                }
                nk_popup_end(ctx);
            }
            else bindings_delete_popup_open = false;
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
        struct {
            const char* title;
            enum bind::action action;
        } actions[] = {
            {"Keys", bind::KEY},
            {"Pitch", bind::FREQUENCY_EXPT},
            {"Volume", bind::VOLUME_MUL},
            {"Modulator period", bind::PERIOD_EXPT},
            {"Modulator amplitude", bind::AMPLITUDE_MUL},
            {"Envelope", bind::ENVELOPE_ADJUST}
        };
        unsigned id = 0;
        for(auto a: actions)
        {
            if(nk_tree_push_id(ctx, NK_TREE_TAB, a.title, NK_MAXIMIZED, id++))
            {
                nk_layout_row_dynamic(ctx, 35, 1);
                int changed_index = -1;
                int movement = 0;
                for(unsigned i = 0; i < binds.bind_count(); ++i)
                {
                    auto& b = binds.get_bind(i);
                    if(b.action != a.action) continue;
                    int ret = gui_bind(b, i);
                    if(ret != 0)
                    {
                        changed_index = i;
                        movement = ret;
                    }
                }
                if(movement)
                    binds.move_bind(changed_index, movement, control, true);
                nk_style_set_font(ctx, &huge_font->handle);
                if(nk_button_symbol(ctx, NK_SYMBOL_PLUS))
                    binds.create_new_bind(a.action);
                nk_style_set_font(ctx, &small_font->handle);
                nk_tree_pop(ctx);
            }
        }

        nk_group_end(ctx);
    }
}

void cafefm::gui_options_editor()
{
    nk_layout_row_template_begin(ctx, 535);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 400);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);

    nk_style_set_font(ctx, &small_font->handle);

    struct nk_rect empty_space;
    nk_widget(&empty_space, ctx);

    // Save, etc. controls at the top
    if(nk_group_begin(ctx, "Options", NK_WINDOW_NO_SCROLLBAR))
    {
        options new_opts(opts);

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 120);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);

        nk_label(ctx, "Audio system: ", NK_TEXT_LEFT);

        std::vector<const char*> systems =
            audio_output::get_available_systems();
        systems.insert(systems.begin(), "Auto");

        new_opts.system_index = nk_combo(
            ctx, systems.data(), systems.size(),
            opts.system_index+1, 25, nk_vec2(260, 200)
        )-1;

        nk_label(ctx, "Output device: ", NK_TEXT_LEFT);

        std::vector<const char*> devices =
            audio_output::get_available_devices(new_opts.system_index);

        new_opts.device_index = nk_combo(
            ctx, devices.data(), devices.size(),
            opts.device_index < 0 ? 0 : opts.device_index, 25, nk_vec2(260, 200)
        );

        nk_label(ctx, "Samplerate: ", NK_TEXT_LEFT);

        std::vector<uint64_t> samplerates =
            audio_output::get_available_samplerates(-1, -1);
        std::vector<std::string> samplerates_str;
        unsigned samplerate_index = 0;
        for(unsigned i = 0; i < samplerates.size(); ++i)
        {
            uint64_t sr = samplerates[i];
            if(sr == opts.samplerate) samplerate_index = i;
            samplerates_str.push_back(std::to_string(sr));
        }
        std::vector<const char*> samplerates_cstr;
        for(std::string& str: samplerates_str)
            samplerates_cstr.push_back(str.c_str());

        new_opts.samplerate = samplerates[nk_combo(
            ctx, samplerates_cstr.data(), samplerates_cstr.size(),
            samplerate_index, 25, nk_vec2(260, 200)
        )];

        nk_label(ctx, "Target latency: ", NK_TEXT_LEFT);

        int milliseconds = round(opts.target_latency*1000.0);
        nk_property_int(ctx, "#Milliseconds:", 0, &milliseconds, 1000, 1, 1);
        new_opts.target_latency = milliseconds/1000.0;

        if(new_opts != opts)
            apply_options(new_opts);

        nk_layout_row_dynamic(ctx, 30, 2);

        if(nk_button_label(ctx, "Save settings"))
            write_options(opts);

        if(nk_button_label(ctx, "Reset settings"))
            apply_options(options());

        if(nk_button_label(ctx, "Open bindings folder"))
            open_bindings_folder();

        if(nk_button_label(ctx, "Open instruments folder"))
            open_instruments_folder();

        nk_layout_row_dynamic(ctx, 30, 1);

        if(nk_button_label(ctx, "Refresh all files"))
        {
            update_compatible_bindings();
            for(unsigned i = 0; i < compatible_bindings.size(); ++i)
            {
                if(compatible_bindings[i].get_name() == binds.get_name())
                {
                    selected_bindings_preset = i;
                    break;
                }
            }
            update_all_instruments();
            for(unsigned i = 0; i < all_instruments.size(); ++i)
            {
                if(all_instruments[i].name == ins_state.name)
                {
                    selected_instrument_preset = i;
                    break;
                }
            }
        }

        static unsigned help_state = 0;
        if(nk_button_label(ctx, "Help")) help_state = 1;

        if(help_state)
        {
            struct nk_rect s = {0, 100, 300, 136};
            s.x = 200-s.w/2;
            const char* help_titles[] = {
                "There is no help.",
                "Press OK.",
                "Go away.",
                "...",
                "-.-",
                "Chapter 1. A game of cards.",
                "Pick one.",
                "Chapter 2. Foul play",
                "I win.",
                "Chapter 3. Go away.",
                "Protip: save often.",
                "P̧͉̩̗Ṛ͈͎̋͊̈ͨ͋A̝͍̠̫̘ͣ̃ͫͮĬ̸̜͖̦̭̙͈̝͂̐́̊ͪ̾Ŝ̟͎̮ͤ͋̓͡E̽̀ͩ̄ͯ̊̅҉̠ ͒͂͢Ẑ̭̘͇̻ͅA͓̻̝̜̮̲ͅL̩̭͇T̴̯͔͈̔́H̿̐ͬ҉̰̣Ōͦ͐͌ͨ͝R̴̝͎̮̜ͣ͆̋̓ͩ͑"
            };

            if(nk_popup_begin(
                ctx, NK_POPUP_STATIC, help_titles[help_state-1],
                NK_WINDOW_BORDER|NK_WINDOW_TITLE, s
            )){
                const char* help_messages[] = {
                    "I have no advice to offer to you.",
                    "No, really. The author didn't bother to write proper "
                    "documentation, so there is nothing to show. Just press "
                    "OK.",
                    "Just press OK. This really goes nowhere.",
                    "Really?",
                    "What would you expect to see?",
                    "Alright, let's play your stupid games. I'll show you four "
                    "cards. Pick one and make a mental note.",
                    "♠5, ♣9, ♦K, ♥Q",
                    "Next, I'll remove exactly the card you were thinking of!",
                    "♣C, ♥J, ♦4",
                    "Wasn't funny? Who's the one clicking cancel just to see "
                    "some shitty jokes?",
                    "Do you really want to see what happens when you push a "
                    "poor program over the edge?",
                    "This."
                };
                nk_layout_row_dynamic(ctx, 50, 1);
                nk_label_wrap(ctx, help_messages[help_state-1]);
                nk_layout_row_dynamic(ctx, 30, 2);
                if(nk_button_label(ctx, "OK"))
                {
                    help_state = 0;
                    nk_popup_close(ctx);
                }
                if(nk_button_label(ctx, "Cancel"))
                {
                    help_state++;
                    nk_popup_close(ctx);
                }
                nk_popup_end(ctx);
            }
            else instrument_delete_popup_open = false;
        }

        nk_group_end(ctx);
    }

    nk_widget(&empty_space, ctx);
    
    // Draw notes at the bottom
    nk_layout_row_dynamic(ctx, 30, 2);

    struct nk_color fade_color = nk_rgb(80,80,80);
    nk_label_colored(
        ctx, "Copyright 2018-2019 Julius Ikkala", NK_TEXT_LEFT, fade_color
    );
    nk_label_colored(
        ctx, "0.1.0 Moldy Bread edition", NK_TEXT_RIGHT, fade_color
    );
}

void cafefm::gui()
{
    int w, h;
    nk_input_end(ctx);

    SDL_GetWindowSize(win, &w, &h);

    const char* tabs[] = {
        "Instrument",
        "Bindings",
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
            gui_instrument_editor();
            break;
        case 1:
            gui_bindings_editor();
            break;
        case 2:
            gui_options_editor();
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
    if(selected_controller)
        set_controller_grab(false);

    selected_controller = c;

    fm->release_all_voices();
    control.reset();
    update_compatible_bindings();
    if(compatible_bindings.size() >= 1)
    {
        selected_bindings_preset = 0;
        binds = compatible_bindings[0];
    }
    else
    {
        selected_bindings_preset = -1;
        create_new_bindings();
    }
}

void cafefm::select_compatible_bindings(unsigned index)
{
    fm->release_all_voices();
    control.reset();
    if(compatible_bindings.size() == 0)
    {
        selected_bindings_preset = -1;
        create_new_bindings();
    }
    else
    {
        selected_bindings_preset = std::min(
            index,
            (unsigned)compatible_bindings.size()-1
        );
        binds = compatible_bindings[selected_bindings_preset];
    }
}

void cafefm::save_current_bindings()
{
    if(selected_controller)
        binds.set_target_device(selected_controller);

    binds.set_write_lock(false);
    write_bindings(binds);
    update_compatible_bindings();

    for(unsigned i = 0; i < compatible_bindings.size(); ++i)
    {
        if(compatible_bindings[i].get_name() == binds.get_name())
        {
            selected_bindings_preset = i;
            break;
        }
    }
}

void cafefm::create_new_bindings()
{
    fm->release_all_voices();
    control.reset();
    selected_bindings_preset = -1;
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
            modified_name = name + std::to_string(++i);
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
    select_compatible_bindings(selected_bindings_preset);
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

    if(compatible_bindings.size() == 0) selected_bindings_preset = -1;
    if(selected_bindings_preset > (int)compatible_bindings.size())
        selected_bindings_preset = compatible_bindings.size() - 1;
}

void cafefm::select_instrument(unsigned index)
{
    if(all_instruments.size() == 0)
    {
        selected_instrument_preset = -1;
        create_new_instrument();
    }
    else
    {
        selected_instrument_preset = std::min(
            index,
            (unsigned)all_instruments.size()-1
        );
        ins_state = all_instruments[selected_instrument_preset];
        reset_fm(false);
        control.apply(*fm, master_volume, ins_state);
    }
}

void cafefm::save_current_instrument()
{
    ins_state.write_lock = false;
    write_instrument(opts.samplerate, ins_state);
    update_all_instruments();

    for(unsigned i = 0; i < all_instruments.size(); ++i)
    {
        if(all_instruments[i].name == ins_state.name)
        {
            selected_instrument_preset = i;
            break;
        }
    }
}

void cafefm::create_new_instrument()
{
    selected_instrument_preset = -1;

    ins_state = instrument_state(opts.samplerate);

    std::string name = "New instrument";
    std::string modified_name = name;
    for(unsigned i = 0; ; ++i)
    {
        modified_name = name;
        if(i > 0) modified_name += " #" + std::to_string(i+1);
        bool found = true;
        for(auto& s: all_instruments)
        {
            if(s.name == modified_name)
            {
                found = false;
                break;
            }
        }
        if(found) break;
    }
    ins_state.name = modified_name;

    reset_fm(false);
    control.apply(*fm, master_volume, ins_state);
}

void cafefm::delete_current_instrument()
{
    remove_instrument(ins_state);
    update_all_instruments();
    select_instrument(selected_instrument_preset);
}

void cafefm::update_all_instruments()
{
    all_instruments = load_all_instruments(opts.samplerate);
}

void cafefm::reset_fm(bool refresh_only)
{
    if(output) output->stop();
    output.reset(nullptr);

    std::unique_ptr<fm_instrument> new_fm;
    
    new_fm.reset(ins_state.create_instrument(opts.samplerate));

    if(!refresh_only) master_volume = 1.0/ins_state.polyphony;
    new_fm->set_volume(master_volume);

    if(fm) new_fm->copy_state(*fm);

    fm.swap(new_fm);
    output.reset(
        new audio_output(
            *fm,
            opts.target_latency,
            opts.system_index,
            opts.device_index
        )
    );
    output->start();
}

void cafefm::apply_options(const options& new_opts)
{
    ins_state.adsr = ins_state.adsr.convert(
        opts.samplerate, new_opts.samplerate
    );
    for(instrument_state& state: all_instruments)
    {
        state.adsr = state.adsr.convert(opts.samplerate, new_opts.samplerate);
    }

    opts = new_opts;
    reset_fm();
    control.apply(*fm, master_volume, ins_state);
}
