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
#define LOOPS_HEADER_HEIGHT 40
#define LOOP_HEIGHT 40
#define MIN_WINDOW_WIDTH 800
#define MIN_WINDOW_HEIGHT 600
#define MAX_BINDING_NAME_LENGTH 128
#define MAX_INSTRUMENT_NAME_LENGTH 128
#define SIDE_PLUS_SIZE 0.05

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
        double* threshold,
        double* origin
    ){
        struct nk_rect bounds;
        nk_widget_layout_states state;
        state = nk_widget(&bounds, ctx);
        if(!state) return;

        struct nk_command_buffer *out = &ctx->current->buffer;
        struct nk_style_progress* style = &ctx->style.progress;
        struct nk_style_item* background = &style->normal;

        nk_input* in = &ctx->input;

        struct nk_rect click_area = bounds;
        struct nk_rect cursor = bounds;

        // Handle input
        if(
            in &&
            nk_input_has_mouse_click_down_in_rect(
                in, NK_BUTTON_LEFT, click_area, nk_true
            )
        ){
            double click_x = (in->mouse.pos.x - cursor.x) / cursor.w;
            double click_y = (in->mouse.pos.y - cursor.y) / cursor.h;
            click_x = std::min(click_x, 1.0);
            click_x = std::max(click_x, 0.0);
            click_x = is_signed ?  click_x * 2.0 - 1.0 : click_x;
            if(origin == nullptr) *threshold = click_x;
            else if(click_y > 0.5) *origin = click_x;
            else *threshold = fabs(*origin-click_x);
        }

        double normalized_input =
            is_signed ? input_value*0.5 + 0.5 : input_value;
        cursor.w = normalized_input * cursor.w;

        // Draw

        struct nk_color line_color = nk_rgb(240, 0, 0);
        struct nk_color deadzone_color = nk_rgb(0, 0, 240);
        struct nk_color bar_color = style->cursor_normal.data.color;

        if(origin == nullptr)
        {
            if(*threshold < input_value)
                bar_color = nk_rgb(175,0,0);
        }
        else if(fabs(*origin-input_value) < *threshold)
            bar_color = nk_rgb(0,0,175);

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
        if(origin == nullptr)
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
        else
        {
            double deadzone_low = *origin - *threshold;
            double deadzone_high = *origin + *threshold;
            double origin_x = *origin;
            if(is_signed)
            {
                deadzone_low = deadzone_low*0.5 + 0.5;
                deadzone_high = deadzone_high*0.5 + 0.5;
                origin_x = origin_x*0.5 + 0.5;
            }
            deadzone_low = deadzone_low * bounds.w + cursor.x;
            deadzone_high = deadzone_high * bounds.w + cursor.x;
            origin_x = origin_x * bounds.w + cursor.x;

            if(deadzone_low >= cursor.x && deadzone_low <= cursor.x+bounds.w)
            {
                nk_stroke_line(
                    out,
                    deadzone_low, cursor.y, deadzone_low, cursor.y+cursor.h/2,
                    2.0f, deadzone_color
                );
            }
            if(deadzone_high >= cursor.x && deadzone_high <= cursor.x+bounds.w)
            {
                nk_stroke_line(
                    out,
                    deadzone_high, cursor.y, deadzone_high, cursor.y+cursor.h/2,
                    2.0f, deadzone_color
                );
            }
            nk_stroke_line(
                out,
                origin_x, cursor.y+cursor.h/2, origin_x, cursor.y+cursor.h,
                2.0f, line_color
            );
        }
    }

    void metronome_widget(struct nk_context* ctx, double beat_index)
    {
        struct nk_rect bounds;
        if(!nk_widget(&bounds, ctx)) return;

        struct nk_command_buffer* out = &ctx->current->buffer;
        struct nk_style_progress* style = &ctx->style.progress;

        nk_color c = style->normal.data.color;
        nk_color active = style->cursor_normal.data.color;
        double value = pow(1.0-fmod(beat_index, 1.0), 4.0);
        c.r = round(lerp(c.r, active.r, value));
        c.g = round(lerp(c.g, active.g, value));
        c.b = round(lerp(c.b, active.b, value));

        nk_fill_rect(out, bounds, style->rounding, style->normal.data.color);

        float border = 3;
        struct nk_rect inner = bounds;
        inner.x += border;
        inner.y += border;
        inner.w -= 2*border;
        inner.h -= 2*border;

        nk_fill_rect(out, inner, style->rounding, c);
    }

    constexpr const char* const protips[] = {
        "The modulator indices are only useful in bindings. They may change "
        "when you make modifications, so the same modulator may have a "
        "different index if you change the synth.",
        "Pick as small value for polyphony as you can; especially for "
        "keyboards, which often allow only few simultaneous keypresses. This "
        "often lets you set latency a bit lower than high polyphony.",
        "Remember to save often, this program is extremely dangerous and may "
        "crash at any time.",
        "Do or do not, there is no undo.",
        "If you hear clicks or static, first check that the master volume "
        "isn't warning about peaking. If it isn't that, try picking a higher "
        "latency.",
        "Have multiple cores? Sorry, this program isn't using them.",
        "44100 is almost always enough. However, if you do hear artifacts with "
        "high frequency noises, lifting the samplerate could help.",
        "The bit depth outputted by this program is 32 bits and cannot be "
        "changed.",
        "Looking for documentation? You're looking at it right now.",
        "Read the protips, they can often be useful.",
        "Some audio subsystems and devices may have special requirements and "
        "not work on your system or this program.",
        "Modulators shown horizontally are summed to each other, and in "
        "vertical configurations the one below is modulating the one above.",
        "Phase often has minimal effect on the sound, but may sometimes "
        "affect the perceived pitch and timbre slightly.",
        "Looking for keyboard shortcuts? Exhaustive list: Alt + F4.",
        "Setting amplitude above 1 generally breaks things if you have more "
        "than one modulator. Also, try to keep the total amplitude of summed "
        "modulators less than or equal to 1.",
        "If you have issues with some cumulative or stacking bindings going "
        "too far, either click the reset button at the top of the screen or "
        "add an opposite cumulative or stacking binding to another axis or "
        "button.",
        "Adjust the threshold of an axis binding by dragging the red line.",
        "Adjust the offset of a continuous axis binding by dragging the blue "
        "line. The deadzone can then be adjusted by dragging the red lines.",
        "Controller axis slightly off-center? No worries, just move the red "
        "origin marker to the real center point and adjust the blue markers "
        "above it to set the deadzone.",
        "To share instruments and bindings with others, press the \"Open "
        "folder\" buttons above and pick the ones you want to share. Send "
        "those files to them, and instruct them to open the same folder and "
        "put the files there.",
        "To find your recordings, click the \"Open recordings folder\" button "
        "above.",
        "Delete a modulator by clicking the [x] button on its title bar. "
        "Notice that this will also delete its modulators as well, and may "
        "shuffle some indices of other modulators.",
        "The help button doesn't like being pressed, so leave it in peace."
    };
};

cafefm::cafefm()
:   win(nullptr), bindings_delete_popup_open(false),
    instrument_delete_popup_open(false), save_recording_state(0),
    selected_controller(nullptr), keyboard_grabbed(false), mouse_grabbed(false),
    master_volume(0.5)
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
        MIN_WINDOW_WIDTH,
        MIN_WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_ALLOW_HIGHDPI|
        SDL_WINDOW_RESIZABLE
    );
    SDL_SetWindowMinimumSize(win, MIN_WINDOW_WIDTH, MIN_WINDOW_HEIGHT);
    ww = MIN_WINDOW_WIDTH;
    wh = MIN_WINDOW_HEIGHT;

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
    protip_index = 0;

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

    // Load icon first, to display it ASAP. Don't load it on Windows, it's not necessary there.
#if !defined(_WIN32) && !defined(WIN32) 
    icon = IMG_Load((icon_dir/"128.png").string().c_str());
    if(icon) SDL_SetWindowIcon(win, icon);
#endif

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
    SDL_SetWindowSize(
        win,
        ww = opts.initial_window_width,
        wh = opts.initial_window_height
    );

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
    save_recording_state = 0;

    nk_sdl_destroy_texture(yellow_warn_img.handle.id);
    nk_sdl_destroy_texture(gray_warn_img.handle.id);

#if !defined(_WIN32) && !defined(WIN32) 
    SDL_FreeSurface(icon);
#endif
}

void cafefm::render()
{
    SDL_GetWindowSize(win, &ww, &wh);

    gui();

    glViewport(0, 0, ww, wh);
    glClearColor(0,0,0,0);
    glClear(GL_COLOR_BUFFER_BIT);
    nk_sdl_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_MEMORY, MAX_ELEMENT_MEMORY);
    SDL_GL_SwapWindow(win);
}

bool cafefm::update(unsigned dt)
{
    bool quit = false;

    // Discover midi inputs
    auto midi_controllers = midi.discover();
    for(auto& c: midi_controllers) available_controllers.emplace_back(c);

    auto cb = [this](
        controller* c, int axis_1d_index, int axis_2d_index, int button_index
    ){
        handle_controller(c, axis_1d_index, axis_2d_index, button_index);
    };

    // Handle controllers that poll themselves
    for(unsigned i = 0; i < available_controllers.size(); ++i)
    {
        auto& c = available_controllers[i];
        auto fn = c.get() == selected_controller ?
            cb : controller::change_callback();
        if(!c->poll(fn)) detach_controller(i);
    }

    // Handle SDL-related controllers
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
        case SDL_WINDOWEVENT:
            if(e.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                render();
                opts.initial_window_width = ww;
                opts.initial_window_height = wh;
            }
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

    // Apply controls
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

    binds.act(
        c,
        control,
        output ? &output->get_looper() : nullptr,
        axis_1d_index,
        axis_2d_index,
        button_index
    );
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
        "Saw",
        "Noise"
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
    return old_type != type ? CHANGE_REQUIRE_IMPORT : CHANGE_NONE;
}

unsigned cafefm::gui_modulation_mode()
{
    static const char* mode_labels[] = {
        "Frequency", "Phase"
    };

    fm_synth::modulation_mode mode = ins_state.synth.get_modulation_mode();
    fm_synth::modulation_mode old_mode = mode;
    mode = (fm_synth::modulation_mode)nk_combo(
        ctx,
        mode_labels,
        sizeof(mode_labels)/sizeof(const char*),
        old_mode,
        20,
        nk_vec2(180, 200)
    );
    ins_state.synth.set_modulation_mode(mode);
    return old_mode != mode ? CHANGE_REQUIRE_IMPORT : CHANGE_NONE;
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
        nk_layout_row_template_push_static(ctx, 230);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, ww/4);
        nk_layout_row_template_end(ctx);

        // Waveform type selection
        nk_style_set_font(ctx, &small_font->handle);
        if(nk_group_begin(ctx, "Carrier Waveform", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_template_begin(ctx, 30);
            nk_layout_row_template_push_static(ctx, 90);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);
            nk_label(ctx, "Waveform:", NK_TEXT_LEFT);
            mask |= gui_oscillator_type(type);
            nk_label(ctx, "Modulation:", NK_TEXT_LEFT);
            mask |= gui_modulation_mode();

            nk_layout_row_dynamic(ctx, 30, 1);
            nk_property_double(
                ctx,
                "#Tuning (Hz)",
                220.0f,
                &ins_state.tuning_frequency,
                880.0f, 0.5f, 0.5f
            );

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
    bool& erase,
    unsigned partition,
    bool down
){
    unsigned mask = CHANGE_NONE;

    nk_style_set_font(ctx, &small_font->handle);
    std::string title = "Modulator " + std::to_string(index);
    nk_window_show(ctx, title.c_str(), NK_SHOWN);
    int res = nk_group_begin(
        ctx, title.c_str(),
        NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER|
        NK_WINDOW_TITLE|NK_WINDOW_CLOSABLE
    );
    if(res == NK_WINDOW_HIDDEN)
    {
        erase = true;
        mask |= CHANGE_REQUIRE_RESET;
    }
    else if(res)
    {
        switch(partition)
        {
        case 1:
            nk_layout_row_template_begin(ctx, 30);
            nk_layout_row_template_push_static(ctx, 100);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);
            break;
        case 2:
            nk_layout_row_template_begin(ctx, 30);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);
            break;
        default:
            nk_layout_row_dynamic(ctx, 30, 1);
            break;
        }
        // Waveform type selection
        nk_style_set_font(ctx, &small_font->handle);
        oscillator::func type = osc.get_type();
        mask |= gui_oscillator_type(type, down);
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
        
        nk_group_end(ctx);
    }

    return mask;
}

void cafefm::gui_instrument_editor()
{
    unsigned mask = CHANGE_NONE;

    nk_layout_row_dynamic(ctx, 142, 1);

    nk_style_set_font(ctx, &small_font->handle);

    // Save, etc. controls at the top
    if(nk_group_begin(
        ctx, "Instrument Control", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        float max_safe_volume = 1.0/ins_state.polyphony;

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 80);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, 90);
        nk_layout_row_template_push_static(ctx, 184);
        nk_layout_row_template_end(ctx);

        // Preset picker
        nk_label(ctx, "Preset:", NK_TEXT_LEFT);
        std::string combo_label = "(None)";
        if(selected_instrument_preset >= 0)
            combo_label = all_instruments[selected_instrument_preset].name;

        if(nk_combo_begin_label(ctx, combo_label.c_str(), nk_vec2(ww-400,200)))
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
        nk_layout_row_template_push_static(ctx, ww-316);
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
            s.x = ww/2-s.w/2;
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
        nk_layout_row_template_push_static(ctx, ww-460);
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
        nk_layout_row_template_push_static(ctx, ww-460);
        nk_layout_row_template_push_dynamic(ctx);
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

        if(
            save_recording_state == 1 &&
            (nk_button_label(ctx, "Finish recording") ||
             !output->is_recording())
        ){
            output->stop_recording();
            save_recording_state = 2;
        }
        else if(nk_button_label(ctx, "Start recording"))
        {
            save_recording_state = 1;
            output->start_recording(
                opts.recording_format,
                opts.recording_quality
            );
        }

        if(save_recording_state >= 2)
        {
            struct nk_rect s = {0, 100, 300, 136};
            s.x = ww/2-s.w/2;
            if(nk_popup_begin(
                ctx, NK_POPUP_STATIC, "Save recording?",
                NK_WINDOW_BORDER|NK_WINDOW_TITLE, s
            )){
                if(save_recording_state == 2)
                {
                    std::string save_text =
                        "Do you want to save the previous recording?";
                    nk_layout_row_dynamic(ctx, 50, 1);
                    nk_label_wrap(ctx, save_text.c_str());
                    nk_layout_row_dynamic(ctx, 30, 2);
                    if (nk_button_label(ctx, "Save"))
                    {

                        if(!output->is_encoding())
                        {
                            save_recording_state = 0;
                            write_recording(output->get_encoder());
                        }
                        else save_recording_state = 3;
                        nk_popup_close(ctx);
                    }
                    if (nk_button_label(ctx, "Cancel"))
                    {
                        output->abort_encoding();
                        save_recording_state = 0;
                        nk_popup_close(ctx);
                    }
                }
                else if(save_recording_state == 3)
                {
                    std::string save_text = "Please wait, encoding...";
                    nk_layout_row_dynamic(ctx, 50, 1);
                    nk_label_wrap(ctx, save_text.c_str());
                    nk_layout_row_dynamic(ctx, 30, 1);
                    uint64_t num, denom;
                    output->get_encoding_progress(num, denom);
                    nk_progress(ctx, &num, denom, NK_FIXED);
                    if(!output->is_encoding())
                    {
                        save_recording_state = 0;
                        write_recording(output->get_encoder());
                    }
                }
                nk_popup_end(ctx);
            }
            else save_recording_state = 0;
        }

        nk_group_end(ctx);
    }

    nk_style_set_font(ctx, &small_font->handle);

    float control_height =
        wh-nk_widget_position(ctx).y-ctx->style.window.group_padding.y*2;
    nk_layout_row_dynamic(ctx, control_height, 1);
    struct nk_rect empty_space;
    if(nk_group_begin(ctx, "Synth Control", NK_WINDOW_BORDER))
    {
        // Render carrier & ADSR
        oscillator::func carrier = ins_state.synth.get_carrier_type();
        mask |= gui_carrier(carrier);
        ins_state.synth.set_carrier_type(carrier);

        fm_synth::layout layout = ins_state.synth.generate_layout();

        int erase_index = -1;
        int add_parent = -2;
        std::map<int, double> modulator_width;
        modulator_width[-1] = 1.0;

        for(fm_synth::layout::layer& layer: layout.layers)
        {
            unsigned max_partition = 0;
            unsigned row_elements = 0;
            for(fm_synth::layout::group& group: layer)
            {
                if(group.partition > max_partition)
                    max_partition = group.partition;
                // Add room for each modulator
                row_elements += group.modulators.size();
                // Add room for +-buttons
                if(
                    group.modulators.size() == 0 ||
                    group.partition < 4
                ) row_elements++;
            }
            if(max_partition == 0) continue;
            // Determine layer height based on max partition.
            constexpr unsigned partition_height[] = {
                73, 108, 175
            };
            unsigned height = partition_height[std::min(max_partition-1, 2u)];

            nk_layout_row_begin(ctx, NK_DYNAMIC, height, row_elements);
            for(fm_synth::layout::group& group: layer)
            {
                if(group.modulators.size() == 0)
                {
                    // Compute width from parent width.
                    nk_layout_row_push(ctx, modulator_width[group.parent]);
                    if(group.empty) nk_widget(&empty_space, ctx);
                    else
                    {
                        nk_style_set_font(ctx, &huge_font->handle);
                        if(nk_button_symbol(ctx, NK_SYMBOL_PLUS))
                        {
                            add_parent = group.parent;
                        }
                        nk_style_set_font(ctx, &small_font->handle);
                    }
                }
                else
                {
                    double width = modulator_width[group.parent];
                    if(group.partition < 4) width -= SIDE_PLUS_SIZE;
                    width /= group.modulators.size();
                    for(unsigned m: group.modulators)
                    {
                        modulator_width[m] = width;
                        nk_layout_row_push(ctx, width);
                        bool erase = false;
                        mask |= gui_modulator(
                            ins_state.synth.get_modulator(m),
                            m,
                            erase,
                            group.partition,
                            true
                        );
                        if(erase) erase_index = m;
                    }
                    if(group.partition < 4)
                    {
                        if(group.modulators.size() > 0)
                        {
                            modulator_width[group.modulators.back()] += SIDE_PLUS_SIZE;
                        }
                        nk_layout_row_push(ctx, SIDE_PLUS_SIZE);
                        nk_style_set_font(ctx, &huge_font->handle);
                        if(nk_button_symbol(ctx, NK_SYMBOL_PLUS))
                        {
                            add_parent = group.parent;
                        }
                        nk_style_set_font(ctx, &small_font->handle);
                    }
                }
            }
            nk_layout_row_end(ctx);
        }

        // Do add & erase here to avoid meddling with layouts and such.
        if(add_parent >= -1)
        {
            unsigned i = ins_state.synth.add_modulator(
                {oscillator::SINE, 1.0, 0.5}
            );
            if(add_parent >= 0)
            {
                ins_state.synth.get_modulator(add_parent)
                    .get_modulators().push_back(i);
            }
            else
            {
                ins_state.synth.get_carrier_modulators().push_back(i);
            }
            mask |= CHANGE_REQUIRE_RESET;
        }

        if(erase_index >= 0) ins_state.synth.erase_modulator(erase_index);

        nk_group_end(ctx);
    }

    // Do necessary updates
    if(mask & CHANGE_REQUIRE_RESET)
    {
        ins_state.synth.finish_changes();
        reset_fm();
    }
    else if(mask & CHANGE_REQUIRE_IMPORT)
    {
        control.apply(*fm, master_volume, ins_state);
        fm->refresh_all_voices();
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
    case bind::LOOP_CONTROL:
        nk_layout_row_template_push_static(ctx, 90);
        nk_layout_row_template_push_static(ctx, 90);
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

    static const char* loop_control_names[] = {
        "Record", "Clear", "Mute"
    };

    switch(b.action)
    {
    case bind::KEY:
        {
            std::string note_name = generate_semitone_name(b.key_semitone);

            bool was_open = ctx->current->popup.win;
            if(nk_combo_begin_label(ctx, note_name.c_str(), nk_vec2(80, -200)))
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
            (int*)&b.envelope.which, 20, nk_vec2(80, 105)
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
    case bind::LOOP_CONTROL:
        {
            bind::loop_control old_control = b.loop.control;
            nk_combobox(
                ctx, loop_control_names,
                sizeof(loop_control_names)/sizeof(*loop_control_names),
                (int*)&b.loop.control, 20, nk_vec2(80, -105)
            );
            if(b.loop.control != old_control)
            {
                if(b.loop.control == bind::LOOP_RECORD) b.toggle = true;
                else if(b.loop.control == bind::LOOP_CLEAR) b.toggle = false;
                else if(b.loop.control == bind::LOOP_MUTE) b.toggle = true;
            }

            std::vector<std::string> labels;
            int offset = 2;
            switch(b.loop.control)
            {
            case bind::LOOP_RECORD:
                offset = 1;
                labels.push_back("Next");
                if(b.loop.index == -2) b.loop.index = -1;
                break;
            case bind::LOOP_CLEAR:
                labels.push_back("All");
                labels.push_back("Previous");
                break;
            case bind::LOOP_MUTE:
                labels.push_back("All");
                labels.push_back("Current");
                break;
            }

            unsigned valid_indices = output->get_looper().get_loop_count();
            for(unsigned i = 0; i < valid_indices; ++i)
                labels.push_back(std::to_string(i));

            std::string title;
            if(b.loop.index < (int)labels.size())
                title = labels[offset + b.loop.index];
            else title = std::to_string(b.loop.index);

            if(nk_combo_begin_label(ctx, title.c_str(), nk_vec2(80, -200)))
            {
                nk_layout_row_dynamic(ctx, 30, 1);
                for(unsigned i = 0; i < labels.size(); ++i)
                {
                    if(nk_combo_item_label(
                        ctx, labels[i].c_str(), NK_TEXT_LEFT
                    )) b.loop.index = i-offset;
                }

                nk_combo_end(ctx);
            }
        }
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

    unsigned total = 
        (int)allow_toggle +
        (int)allow_cumulative +
        (int)allow_stacking +
        (int)allow_threshold +
        (int)allow_invert;
    bool multiple = total > 1;

    if(total == 0)
    {
        struct nk_rect empty_space;
        nk_widget(&empty_space, ctx);
        return;
    }

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
        bool old_threshold = has_threshold;
        has_threshold = !nk_check_label(ctx, "Threshold", !has_threshold);
        if(has_threshold)
        {
            b.control = bind::AXIS_1D_THRESHOLD;
            if(old_threshold != has_threshold)
                b.axis_1d.threshold = 0.5;
        }
        else
        {
            b.control = bind::AXIS_1D_CONTINUOUS;
            if(old_threshold != has_threshold)
            {
                b.axis_1d.threshold = 0.0;
                b.axis_1d.origin = 0.0;
            }
        }
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
    {
        bool old_invert = b.axis_1d.invert;
        b.axis_1d.invert = !nk_check_label(ctx, "Invert", !b.axis_1d.invert);
        if(old_invert != b.axis_1d.invert)
        {
            if(b.control == bind::AXIS_1D_CONTINUOUS)
            {
                bool is_signed;
                b.input_value(selected_controller, &is_signed);
                b.axis_1d.origin =
                    is_signed ? -b.axis_1d.origin : 1 - b.axis_1d.origin;
            }
        }
    }

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
            b.axis_1d.threshold =
                b.control == bind::AXIS_1D_THRESHOLD ? 0.5 : 0.0;
            b.axis_1d.origin = 0.0;
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
            &b.axis_1d.threshold,
            b.control == bind::AXIS_1D_CONTINUOUS ?
                &b.axis_1d.origin : nullptr
        );
    }

    gui_bind_modifiers(
        b,
        b.control != bind::AXIS_1D_CONTINUOUS &&
        (b.action != bind::LOOP_CONTROL || b.loop.control != bind::LOOP_CLEAR),
        b.action != bind::KEY && b.action != bind::LOOP_CONTROL,
        b.action != bind::KEY && b.action != bind::LOOP_CONTROL
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
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, 90);
        nk_layout_row_template_push_static(ctx, 184);
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
            selected_index, 25, nk_vec2(ww-400, 200)
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
        nk_layout_row_template_push_static(ctx, ww-400);
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

        if(nk_combo_begin_label(ctx, combo_label.c_str(), nk_vec2(ww-400,200)))
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
        nk_layout_row_template_push_static(ctx, ww-316);
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
            s.x = ww/2-s.w/2;
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

    float control_height =
        wh-nk_widget_position(ctx).y-ctx->style.window.group_padding.y*2;
    nk_layout_row_dynamic(ctx, control_height, 1);

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
            {"Envelope", bind::ENVELOPE_ADJUST},
            {"Loops", bind::LOOP_CONTROL}
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

void cafefm::gui_loop(unsigned loop_index)
{
    nk_style_set_font(ctx, &small_font->handle);

    looper& lo = output->get_looper();
    looper::loop_state state = lo.get_loop_state(loop_index);
    std::string title = "Loop " + std::to_string(loop_index);

    struct nk_rect empty_space;
    if(nk_group_begin(
        ctx, title.c_str(), NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 50);
        nk_layout_row_template_push_static(ctx, 30);
        nk_layout_row_template_push_static(ctx, 140);
        nk_layout_row_template_push_static(ctx, 140);
        nk_layout_row_template_push_static(ctx, 160);
        nk_layout_row_template_push_static(ctx, 80);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, 60);
        nk_layout_row_template_end(ctx);
        nk_label(ctx, title.c_str(), NK_TEXT_LEFT);

        if(state == looper::RECORDING)
        {
            if(nk_button_symbol(ctx, NK_SYMBOL_RECT_SOLID))
            {
                lo.finish_loop(loop_index);
            }
        }
        else if(nk_button_symbol(ctx, NK_SYMBOL_CIRCLE_SOLID))
        {
            lo.record_loop(loop_index);
        }


        double old_length = lo.get_loop_length(loop_index);
        double new_length = fixed_propertyd(
            ctx, "#Beats:", 0.5, old_length, 1000.0, 1.0, 0.01, 0.001
        );
        if(
            new_length != old_length &&
            (state == looper::PLAYING || state == looper::MUTED)
        ) lo.set_loop_length(loop_index, new_length);

        double old_delay = lo.get_loop_delay(loop_index);
        double new_delay = fixed_propertyd(
            ctx, "#Delay:", 0.0, old_delay, 1000.0, 0.05, 0.01, 0.001
        );
        if(
            new_delay != old_delay &&
            (state == looper::PLAYING || state == looper::MUTED)
        ) lo.set_loop_delay(loop_index, new_delay);

        double old_volume = lo.get_loop_volume(loop_index);
        double new_volume = fixed_propertyd(
            ctx, "#Volume:", 0.0, old_volume, 1.0, 0.05, 0.01, 0.001
        );
        if(old_volume != new_volume)
            lo.set_loop_volume(loop_index, new_volume);

        if(button_label_active(
            ctx,
            state == looper::PLAYING ? "Mute" : "Unmute",
            state == looper::PLAYING || state == looper::MUTED
        )) lo.play_loop(loop_index, state == looper::MUTED);

        nk_widget(&empty_space, ctx);

        if(nk_button_label(ctx, "Clear"))
            lo.clear_loop(loop_index);

        nk_group_end(ctx);
    }
}

void cafefm::gui_loops_editor()
{
    nk_layout_row_dynamic(ctx, LOOPS_HEADER_HEIGHT, 1);
    nk_style_set_font(ctx, &small_font->handle);

    looper& lo = output->get_looper();

    // Save, etc. controls at the top
    struct nk_rect empty_space;
    if(nk_group_begin(
        ctx, "Loops Control", NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_BORDER
    )){
        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 120);
        nk_layout_row_template_push_static(ctx, 60);
        nk_layout_row_template_push_static(ctx, 120);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_push_static(ctx, 120);
        nk_layout_row_template_push_static(ctx, 90);
        nk_layout_row_template_push_static(ctx, 184);
        nk_layout_row_template_end(ctx);

        int bpm = round(lo.get_loop_bpm());
        int old_bpm = bpm;
        nk_property_int(ctx, "#BPM:", 1, &bpm, 1000, 1, 1);
        if(bpm != old_bpm) lo.set_loop_bpm(bpm);

        metronome_widget(ctx,  lo.get_loop_beat_index());

        opts.start_loop_on_sound = !nk_check_label(
            ctx, "Start on sound", !opts.start_loop_on_sound
        );
        output->get_looper().set_record_on_sound(opts.start_loop_on_sound);

        nk_widget(&empty_space, ctx);

        if(nk_button_label(ctx, "Clear all loops"))
            lo.clear_all_loops();

        if(nk_button_label(ctx, "Reset"))
        {
            fm->release_all_voices();
            control.reset();
        }

        gui_controller_manager();

        nk_group_end(ctx);
    }

    float loops_height =
        wh-nk_widget_position(ctx).y-ctx->style.window.group_padding.y*2;
    nk_layout_row_dynamic(ctx, loops_height, 1);

    if(nk_group_begin(ctx, "Loops", NK_WINDOW_BORDER))
    {
        for(unsigned i = 0; i < lo.get_loop_count(); ++i)
        {
            nk_layout_row_dynamic(ctx, LOOP_HEIGHT, 1);
            gui_loop(i);
        }
        nk_group_end(ctx);
    }
}

void cafefm::gui_options_editor()
{
    nk_layout_row_template_begin(ctx, 535);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_push_static(ctx, 600);
    nk_layout_row_template_push_dynamic(ctx);
    nk_layout_row_template_end(ctx);

    nk_style_set_font(ctx, &small_font->handle);

    struct nk_rect empty_space;
    nk_widget(&empty_space, ctx);

    // Save, etc. controls at the top
    if(nk_group_begin(ctx, "Options", 0))
    {
        options new_opts(opts);

        nk_layout_row_template_begin(ctx, 30);
        nk_layout_row_template_push_static(ctx, 140);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);

        nk_label(ctx, "Audio system:", NK_TEXT_LEFT);

        std::vector<const char*> systems =
            audio_output::get_available_systems();
        systems.insert(systems.begin(), "Auto");

        new_opts.system_index = nk_combo(
            ctx, systems.data(), systems.size(),
            opts.system_index+1, 25, nk_vec2(440, 200)
        )-1;

        nk_label(ctx, "Output device:", NK_TEXT_LEFT);

        std::vector<const char*> devices =
            audio_output::get_available_devices(new_opts.system_index);

        new_opts.device_index = nk_combo(
            ctx, devices.data(), devices.size(),
            opts.device_index < 0 ? 0 : opts.device_index, 25, nk_vec2(440, 200)
        );

        nk_label(ctx, "Samplerate:", NK_TEXT_LEFT);

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
            samplerate_index, 25, nk_vec2(440, 200)
        )];

        nk_label(ctx, "Target latency:", NK_TEXT_LEFT);

        int milliseconds = round(opts.target_latency*1000.0);
        nk_property_int(ctx, "#Milliseconds:", 0, &milliseconds, 1000, 1, 1);
        new_opts.target_latency = milliseconds/1000.0;

        nk_label(ctx, "Recording format:", NK_TEXT_LEFT);

        new_opts.recording_format = (encoder::format)nk_combo(
            ctx, encoder::format_strings,
            sizeof(encoder::format_strings)/sizeof(*encoder::format_strings),
            (int)opts.recording_format, 25, nk_vec2(440, 200)
        );

        nk_label(ctx, "Recording quality:", NK_TEXT_LEFT);
        float quality = opts.recording_quality;
        float old_quality = quality;
        nk_slider_float(ctx, 0, &quality, 100.0f, 1.00f);
        if(quality != old_quality)
            new_opts.recording_quality = quality;

        if(new_opts != opts)
            apply_options(new_opts);

        nk_layout_row_dynamic(ctx, 30, 2);

        if(nk_button_label(ctx, "Save settings"))
            write_options(opts);

        if(nk_button_label(ctx, "Reset settings"))
            apply_options(options());

        nk_layout_row_dynamic(ctx, 30, 3);

        if(nk_button_label(ctx, "Open bindings folder"))
            open_bindings_folder();

        if(nk_button_label(ctx, "Open instruments folder"))
            open_instruments_folder();

        if(nk_button_label(ctx, "Open recordings folder"))
            open_recordings_folder();

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
            s.x = 300-s.w/2;
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

        nk_layout_row_dynamic(ctx, 150, 1);
        if(nk_group_begin(
            ctx,
            "Protip",
            NK_WINDOW_NO_SCROLLBAR|NK_WINDOW_TITLE|NK_WINDOW_BORDER
        )){
            nk_layout_row_dynamic(ctx, 70, 1);
            nk_label_wrap(ctx, protips[protip_index]);
            nk_layout_row_dynamic(ctx, 30, 1);
            if(nk_button_label(ctx, "Next")) next_protip();
            nk_group_end(ctx);
        }

        nk_group_end(ctx);
    }

    nk_widget(&empty_space, ctx);

    // Padding
    nk_layout_row_dynamic(ctx, wh-nk_widget_position(ctx).y-30, 0);
    
    // Draw notes at the bottom
    nk_layout_row_dynamic(ctx, 30, 2);

    struct nk_color fade_color = nk_rgb(80,80,80);
    nk_label_colored(
        ctx, "Copyright 2018-2019 Julius Ikkala", NK_TEXT_LEFT, fade_color
    );
    nk_label_colored(
        ctx, "0.1.1 Bold Yoghurt edition", NK_TEXT_RIGHT, fade_color
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
        "Loops",
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
        nk_layout_row_begin(ctx, NK_STATIC, 30, tab_count);

        unsigned prev_tab = selected_tab;
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
        if(prev_tab != selected_tab && selected_tab == 2)
        {
            // If the user switched to options page, get a new pro tip.
            next_protip();
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
            gui_loops_editor();
            break;
        case 3:
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
    {
        binds.set_target_device(selected_controller);
        if(selected_controller->get_type_name() == "MIDI input")
        {
            binds = midi_context::generate_default_midi_bindings();
            return;
        }
    }

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
        master_volume = 1.0/ins_state.polyphony;
        reset_fm();
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
    master_volume = 1.0/ins_state.polyphony;

    reset_fm();
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

void cafefm::next_protip()
{
    unsigned prev_protip = protip_index;
    while(protip_index == prev_protip)
        protip_index = rand()%(sizeof(protips)/sizeof(*protips));
}

void cafefm::reset_fm(bool refresh_only)
{
    bool open_output = !refresh_only;
    if(!output || output->get_samplerate() != opts.samplerate)
    {
        output.reset(new audio_output(opts.samplerate));
        open_output = true;
    }
    if(open_output)
        output->open(opts.target_latency, opts.system_index, opts.device_index);

    output->get_looper().set_record_on_sound(opts.start_loop_on_sound);

    std::unique_ptr<fm_instrument> new_fm;
    
    new_fm.reset(ins_state.create_instrument(opts.samplerate));
    new_fm->set_volume(master_volume);

    if(fm) new_fm->copy_state(*fm);
    fm.swap(new_fm);
    control.apply(*fm, master_volume, ins_state);

    output->stop();
    output->set_instrument(*fm);
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
    reset_fm(false);
}
