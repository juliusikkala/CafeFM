#ifndef CAFEFM_CONTROLLER_HH
#define CAFEFM_CONTROLLER_HH
#include "SDL.h"
#include <string>
#include <functional>

template<unsigned D>
struct axis
{
    // If is_signed is set, the values can be negative. The axis should always
    // start from zero. If is_limited is set, the values should be normalized to
    // -1 to 1 or 0 to 1, depending on is_signed.
    bool is_signed, is_limited;
    float values[D];
};

template<>
struct axis<1>
{
    bool is_signed, is_limited;
    float value;
};

using axis_1d = axis<1>;
using axis_2d = axis<2>;

class controller
{
public:
    using change_callback = std::function<void(
        controller* c, int axis_1d_index, int axis_2d_index, int button_index
    )>;

    virtual ~controller();
    // These should return false if controller is disconnected.
    // handle_event is allowed to (and should) ignore unrelated events.
    virtual bool handle_event(const SDL_Event& e, change_callback cb = {});
    virtual bool poll(change_callback cb = {});

    virtual std::string get_type_name() const;
    virtual std::string get_device_name() const;

    virtual unsigned get_axis_1d_count() const;
    virtual std::string get_axis_1d_name(unsigned i) const;
    virtual axis_1d get_axis_1d_state(unsigned i) const;

    virtual unsigned get_axis_2d_count() const;
    virtual std::string get_axis_2d_name(unsigned i) const;
    virtual axis_2d get_axis_2d_state(unsigned i) const;

    virtual unsigned get_button_count() const;
    virtual std::string get_button_name(unsigned i) const;
    virtual unsigned get_button_state(unsigned i) const;

private:
};

#endif
