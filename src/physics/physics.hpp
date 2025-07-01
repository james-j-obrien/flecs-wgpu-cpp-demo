#pragma once

#include "flecs.h"
#include <box2d/box2d.h>

struct DynamicBody {
    float density;
    float friction;
};

struct StaticBody {};

struct Impulse {
    float x;
    float y;
};

namespace physics {

struct BodyPtr {
    b2Body *ptr;
};

struct PhysicsWorld {
    b2World *ptr;
};

struct module {
    module(flecs::world &world);
};

} // namespace physics