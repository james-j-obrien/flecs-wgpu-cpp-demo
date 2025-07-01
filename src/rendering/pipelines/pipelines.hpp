#pragma once

#include "../rendering.hpp"
#include "flecs.h"

namespace quad_pipeline {

struct module {
    module(flecs::world &world);
};
} // namespace quad_pipeline

namespace pipelines {
struct module {
    module(flecs::world &world) { world.import <quad_pipeline::module>(); }
};
} // namespace pipelines