#include "example.hpp"
#include "../../include.hpp"
#include <cpr/cpr.h>
#include <iostream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace images_example {

static float ratio = 4.0f;
static float width = 745.0f / ratio;
static float height = 1040.0f / ratio;
static float rounding = 40.0f / ratio;

static float offset_x = -800.0f;
static float offset_y = -400.0f;
static int per_row = 20;

module::module(flecs::world &world) {
    world.import <rendering::module>();
    world.import <input::module>();
    world.import <http::module>();

    // auto texture = world.entity().set<http::LoadTexture>(
    //     {"https://cards.scryfall.io/png/front/6/d/"
    //      "6da045f8-6278-4c84-9d39-025adf0789c1.png?1562404626"});

    cpr::Url get_cards{"https://api.scryfall.com/cards/search?q=c%3Agreen+mv%3D6"};

    static std::string response{};

    cpr::GetCallback([=](cpr::Response res) { response = res.text; }, get_cards);

    world.system().run([&](flecs::iter &it) {
        flecs::world world{it.world()};
        if (response.length() > 0) {
            auto data = json::parse(response);
            int index = 0;
            for (auto &card : data["data"]) {
                if (index > 300) {
                    break;
                }
                if (!card.contains("image_uris")) {
                    continue;
                }
                auto &uris = card["image_uris"];
                if (!uris.contains("png")) {
                    continue;
                }
                auto url = uris["png"].get<std::string>();

                auto &texture = world.entity().set<http::LoadTexture>({url});

                float x = (float)(index % per_row) * width * 1.2f;
                float y = (float)(index / per_row) * height * 1.2f;

                world.entity()
                    .set<Quad>({width, height, rounding})
                    .set<Position>({x + offset_x, y + offset_y})
                    .add<rendering::RenderTexture>(texture);
                index++;
            }
            response = "";
        }
    });

    // world.entity()
    //     .set<Rectangle>({745.0f, 1040.0f, 40.0f})
    //     .set<Position>({0.0f, 0.0f})
    //     .add<rendering::RenderTexture>(texture);
}
} // namespace images_example