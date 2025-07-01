#pragma once

#include "flecs.h"
#include <string>

namespace http {

struct LoadTexture {
    std::string url;
};

struct module {
    module(flecs::world &world);
};
} // namespace http