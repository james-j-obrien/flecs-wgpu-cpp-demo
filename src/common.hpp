#pragma once

#include <array>
#include <webgpu/webgpu.hpp>
#include <GLFW/glfw3.h>

struct Position {
    float x;
    float y;
    float rotation;
};

struct Quad {
    float width;
    float height;
    float corner_radius = 0.0f;
};

struct Color {
    std::array<float, 4> color;
};

struct Circle {
    float radius;
};

struct Window {
    GLFWwindow *ptr;
    int width;
    int height;
    wgpu::Surface surface = nullptr;

    static void on_remove(Window &value) {
        if (value.surface != nullptr) {
            value.surface.unconfigure();
        }
        glfwDestroyWindow(value.ptr);
        glfwTerminate();
    }
};

struct Input {};
