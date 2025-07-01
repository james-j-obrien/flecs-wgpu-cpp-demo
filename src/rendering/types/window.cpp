#include "../../include.hpp"
#include "types.hpp"
#include <iostream>
#include <glfw3webgpu.h>

namespace window {

struct Resize {
    int width;
    int height;
};

void resize_callback(GLFWwindow *window, int width, int height) {
    flecs::world world{(flecs::world_t *)glfwGetWindowUserPointer(window)};
    world.entity<Window>().emit<Resize>({width, height});
}

module::module(flecs::world &world) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW!" << std::endl;
        world.quit();
        return;
    }

    int width = 640;
    int height = 480;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(width, height, "flecs-wgpu", NULL, NULL);
    if (!window) {
        std::cerr << "Could not open window!" << std::endl;
        glfwTerminate();
        world.quit();
        return;
    }

    world.set<Window>({window, width, height});
    glfwSetWindowUserPointer(window, (void *)world.c_ptr());
    glfwSetFramebufferSizeCallback(window, resize_callback);

    auto window_e = world.entity<Window>();
    window_e.observe<Resize>([](flecs::entity e, Resize &resize) {
        flecs::world world{e.world()};
        auto window = e.get_mut<Window>();
        window.width = resize.width;
        window.height = resize.height;

        world.set<rendering::Uniforms>({(float)resize.width, (float)resize.height});

        auto &webgpu = world.ensure<rendering::WGPU>();

        using namespace wgpu;

        // if (resize.width > 0 && resize.height > 0) {
        TextureFormat format = TextureFormat::BGRA8Unorm;
        SurfaceConfiguration config;
        config.nextInChain = nullptr;
        config.width = resize.width;
        config.height = resize.height;
        config.format = format;
        config.viewFormatCount = 0;
        config.viewFormats = nullptr;
        config.usage = TextureUsage::RenderAttachment;
        config.device = webgpu.device;
        config.presentMode = PresentMode::Fifo;
        config.alphaMode = WGPUCompositeAlphaMode_Auto;
        window.surface.configure(config);
        // } else {
        //     webgpu.swap_chain = nullptr;
        // }
    });

    world.system<Window>().kind(flecs::PreFrame).each([=](flecs::entity e, Window &window) {
        flecs::world world = e.world();

        glfwPollEvents();
        if (glfwWindowShouldClose(window.ptr)) {
            return world.quit();
        }
    });
}
} // namespace window
