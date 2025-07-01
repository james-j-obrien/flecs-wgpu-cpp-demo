#include <webgpu/webgpu.hpp>
#include "flecs.h"
#include "types.hpp"

using namespace rendering;

namespace buffer {

wgpu::Buffer init_buffer(WGPU &webgpu, wgpu::BufferUsage usage) {
    using namespace wgpu;

    BufferDescriptor buffer_desc;
    buffer_desc.usage = usage;
    buffer_desc.mappedAtCreation = false;
    return webgpu.device.createBuffer(buffer_desc);
}

module::module(flecs::world &world) {
    world.component<VertexBuffer>().on_add([](flecs::entity e, VertexBuffer &) {
        e.set<Buffer>(
            {(wgpu::BufferUsage::W)(wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst), 18,
             sizeof(float)});
    });

    world.observer<WGPU, Buffer>()
        .event(flecs::OnSet)
        .term_at(0)
        .singleton()
        .without<Ready>()
        .each([](flecs::entity e, WGPU &webgpu, Buffer &buffer) {
            if (buffer.buffer == nullptr) {
                buffer.init_buffer(webgpu, 4 * 18);
            }
            e.add<Ready>();
        });
}
} // namespace buffer