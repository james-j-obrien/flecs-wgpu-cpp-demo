#pragma once

#include "flecs.h"

struct MousePress {
    float x;
    float y;
};

struct MouseRelease {
    float x;
    float y;
};

namespace input {
struct module {
    module(flecs::world &world);
};
} // namespace input