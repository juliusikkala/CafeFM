#ifndef CAFEFM_CONTROLLER_JOYSTICK_HH
#define CAFEFM_CONTROLLER_JOYSTICK_HH
#include "controller.hh"
#include <memory>
#include <map>

class joystick: public controller
{
public:
    joystick(int device_index);
    joystick(const joystick& other) = delete;
    joystick(joystick&& other) = delete;
    ~joystick();

    bool handle_event(const SDL_Event& e, change_callback cb = {}) override;

    std::string get_type_name() const override;
    std::string get_device_name() const override;

    unsigned get_axis_1d_count() const override;
    std::string get_axis_1d_name(unsigned i) const override;
    axis_1d get_axis_1d_state(unsigned i) const override;

    unsigned get_button_count() const override;
    std::string get_button_name(unsigned i) const override;
    unsigned get_button_state(unsigned i) const override;

private:
    SDL_Joystick* js;
    SDL_JoystickID id;

    mutable std::map<int, SDL_Point> ball_states;
    mutable std::map<int, uint8_t> hat_states;
};

#endif

