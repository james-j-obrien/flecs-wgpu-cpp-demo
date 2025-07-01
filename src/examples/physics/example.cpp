#include "example.hpp"
#include "../../include.hpp"

#include <iostream>

namespace physics_example {

int x_amount = 5;
int y_amount = 10;

struct Holding {};

float get_color(int value, int amount) { return ((float)value / (float)amount + 1.0f) / 2.0f; }

module::module(flecs::world &world) {
    world.import <rendering::module>();
    world.import <physics::module>();
    world.import <input::module>();

    world.entity()
        .set<Quad>({200.0f, 10.0f})
        .set<Color>({0.0f, 0.0f, 0.0f, 1.0f})
        .set<Position>({0.0f, -200.0f, 0.0f})
        .add<StaticBody>();

    for (auto x = -x_amount; x <= x_amount; x++) {
        for (auto y = -y_amount; y <= y_amount; y++) {
            world.entity()
                .set<Quad>({
                    8.0f,
                    8.0f,
                })
                .set<Color>({0.5f, get_color(x, x_amount), get_color(y, y_amount), 1.0f})
                .set<Position>({x * 16.0f, y * 16.0f, 0.0f})
                .set<DynamicBody>({1.0f, 0.3f});
        }
    }

    world.entity<Input>()
        .observe<MousePress>([](flecs::entity e, MousePress &event) {
            flecs::world world = e.world();
            if (world.has<Holding>()) {
                return;
            }

            auto circle = world.entity()
                              .set<Circle>({10.0f})
                              .set<Color>({0.8f, 0.3f, 0.3f, 1.0f})
                              .set<Position>({event.x, event.y, 0.0f});

            world.add<Holding>(circle);
        })
        .observe<MouseRelease>([](flecs::entity e, MouseRelease &event) {
            flecs::world world = e.world();
            if (!world.has<Holding>(flecs::Wildcard)) {
                return;
            }

            auto circle = world.target<Holding>();
            auto pos = circle.get<Position>();

            circle.set<DynamicBody>({1.0f, 0.3f}).set<Impulse>({pos.x - event.x, pos.y - event.y});

            world.remove<Holding>(circle);
        });
}
} // namespace physics_example