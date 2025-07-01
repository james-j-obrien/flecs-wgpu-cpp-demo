#pragma once

#include "../rendering.hpp"

namespace window {

struct module {
    module(flecs::world &world);
};

}; // namespace window

namespace pipeline {

struct module {
    module(flecs::world &world);
};

} // namespace pipeline

namespace buffer {

struct module {
    module(flecs::world &world);
};

} // namespace buffer

namespace types {
struct module {
    module(flecs::world &world) {
        world.import <buffer::module>();
        world.import <pipeline::module>();
    }
};
} // namespace types