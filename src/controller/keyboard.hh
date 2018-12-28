#ifndef CAFEFM_CONTROLLER_KEYBOARD_HH
#define CAFEFM_CONTROLLER_KEYBOARD_HH
#include "controller.hh"
#include <memory>

class keyboard: public controller
{
public:
    bool handle_event(const SDL_Event& e, change_callback cb = {}) override;

    std::string get_type_name() const override;
    std::string get_device_name() const override;

    unsigned get_button_count() const override;
    std::string get_button_name(unsigned i) const override;
    bool get_button_state(unsigned i) const override;
};

#endif
