#include "input.hpp"
#include "../common.hpp"
#include <iostream>
#include <GLFW/glfw3.h>

namespace input {

void mouse_button_callback(GLFWwindow *ptr, int /*button*/, int action, int /*mods*/) {
    flecs::world world{(flecs::world_t *)glfwGetWindowUserPointer(ptr)};
    auto input = world.entity<Input>();
    double x, y;
    glfwGetCursorPos(ptr, &x, &y);

    auto &window = world.ensure<Window>();
    x -= window.width / 2.0;
    y -= window.height / 2.0;

    if (action == GLFW_PRESS) {
        input.emit<MousePress>({(float)x, -(float)y});
    }
    if (action == GLFW_RELEASE) {
        input.emit<MouseRelease>({(float)x, -(float)y});
    }
}

module::module(flecs::world &world) {
    auto &window = world.ensure<Window>();
    world.add<Input>();
    glfwSetMouseButtonCallback(window.ptr, mouse_button_callback);
}
} // namespace input