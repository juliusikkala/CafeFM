#include "keyboard.hh"

bool keyboard::handle_event(const SDL_Event& e, change_callback cb)
{
    switch(e.type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if(!e.key.repeat && cb) cb(this, -1, -1, e.key.keysym.scancode);
        break;
    }
    return true;
}

std::string keyboard::get_type_name() const
{
    return "Keyboard";
}

std::string keyboard::get_device_name() const
{
    return "Keyboard";
}

unsigned keyboard::get_button_count() const
{
    return SDL_NUM_SCANCODES;
}

std::string keyboard::get_button_name(unsigned i) const
{
    return SDL_GetScancodeName((SDL_Scancode)i);
}

unsigned keyboard::get_button_state(unsigned i) const
{
    const uint8_t* state = SDL_GetKeyboardState(NULL);
    return state[i];
}
