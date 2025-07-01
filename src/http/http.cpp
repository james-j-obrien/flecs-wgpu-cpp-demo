#include "flecs.h"

#include "../rendering/rendering.hpp"
#include "../rendering/stb_image.h"
#include "http.hpp"
#include <cpr/cpr.h>
#include <iostream>
#include <webgpu/webgpu.hpp>

namespace http {

struct LoadedImages {
    std::mutex mutex;
    std::vector<flecs::entity_t> entities;
    std::vector<rendering::ImageData> data;
};

module::module(flecs::world &world) {

    static LoadedImages images{};

    world.observer<LoadTexture>()
        .event(flecs::OnSet)
        .each([](flecs::entity e, LoadTexture &texture) {
            cpr::Url url{texture.url};

            cpr::GetCallback(
                [=](cpr::Response res) {
                    // std::cout << "Downloaded: " << res.url << std::endl;
                    int x, y, n;
                    auto data = stbi_load_from_memory((stbi_uc *)res.text.c_str(),
                                                      (int)res.text.length(), &x, &y, &n, 0);
                    // std::cout << "Size: " << x << ", " << y << std::endl;

                    std::lock_guard guard{images.mutex};
                    images.entities.push_back(e.id());
                    images.data.push_back({(unsigned int)x, (unsigned int)y, data});
                },
                url);
        });

    world.system().run([](flecs::iter &it) {
        if (!images.mutex.try_lock()) {
            return;
        }
        std::lock_guard guard(images.mutex, std::adopt_lock);
        for (auto i = 0; i < images.entities.size(); i++) {
            it.world()
                .entity(images.entities[i])
                .set<rendering::ImageData>(std::move(images.data[i]));
        }
        images.entities.clear();
        images.data.clear();
    });
}
} // namespace http