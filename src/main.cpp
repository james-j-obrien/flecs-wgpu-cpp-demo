#include "examples/physics/example.hpp"
#include "flecs.h"
#include <iostream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif // __EMSCRIPTEN__

int main(void) {
    flecs::world world{ecs_init()};
    world.import <physics_example::module>();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(
        [](void *world_ptr) {
            flecs::world &world = *reinterpret_cast<flecs::world *>(world_ptr);
            world.progress();
        },
        (void *)&world, 0, true);

    return 0;
#else  // __EMSCRIPTEN__
    flecs::app_builder app{world};
    app.enable_rest();
    app.target_fps(60.0);
    return app.run();
#endif // __EMSCRIPTEN__
}
