#pragma once

#include "../rendering.hpp"
#include "flecs.h"
#include <glm/glm.hpp>

namespace quad_pipeline {

struct module {
    module(flecs::world &world);
};
} // namespace quad_pipeline

namespace image_pipeline {

struct module {
    module(flecs::world &world);
};
} // namespace image_pipeline

namespace pipelines {
struct module {
    module(flecs::world &world) {
        world.import <quad_pipeline::module>();
        // world.import <image_pipeline::module>();
    }
};
} // namespace pipelines