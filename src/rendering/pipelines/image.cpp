#include "../../common.hpp"
#include "flecs.h"
#include "pipelines.hpp"
#include <array>
#include <webgpu/webgpu.hpp>

using namespace rendering;

namespace image_pipeline {

struct BufferData {
    std::array<float, 10> data;
    std::array<uint32_t, 2> padding;
};

struct InstanceBuffer {
    std::vector<BufferData> buffer;
};

std::vector<float> quad_vertices = {
    -1.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, -1.0, 0.0, 1.0, -1.0, 0.0, -1.0, -1.0, 0.0, -1.0, 1.0, 0.0,
};

wgpu::VertexBufferLayout init_vertex_layout() {
    using namespace wgpu;

    VertexAttribute *attrib = new VertexAttribute();
    attrib->shaderLocation = 0;
    attrib->format = VertexFormat::Float32x3;
    attrib->offset = 0;

    VertexBufferLayout layout;
    layout.attributeCount = 1;
    layout.attributes = attrib;
    layout.arrayStride = 3 * sizeof(float);
    layout.stepMode = VertexStepMode::Vertex;

    return layout;
};

wgpu::BindGroupLayout init_instance_bind_group_layout(WGPU &webgpu) {
    using namespace wgpu;

    BindGroupLayoutEntry storage_entry{};
    storage_entry.binding = 0;
    storage_entry.buffer.type = BufferBindingType::ReadOnlyStorage;
    storage_entry.visibility = ShaderStage::Vertex;

    BindGroupLayoutDescriptor layout_desc;
    layout_desc.entryCount = 1;
    layout_desc.entries = &storage_entry;
    layout_desc.label = toWgpuStringView("Image Instance Bind Group Layout");

    return webgpu.device.createBindGroupLayout(layout_desc);
}

struct ImageBinding {};

struct BoundTextures {};

struct BoundInstances {};

module::module(flecs::world &world) {
    auto &webgpu = world.ensure<WGPU>();
    auto vertex_layout = init_vertex_layout();
    auto instance_layout = init_instance_bind_group_layout(webgpu);
    auto &uniform_layout = world.entity<Uniforms>().get<BindingLayout>().layout.value();
    auto &image_layout = world.entity<TextureArrayCache>().get<BindingLayout>().layout.value();

    auto &pipeline =
        world.entity()
            .set<Shader>({ASSET_DIR "/shaders/image.wgsl"})
            .set<RenderPipeline>({vertex_layout, {uniform_layout, instance_layout, image_layout}});

    auto &vertex_buffer = world.entity().set<VertexBuffer>({vertex_layout});

    world.observer<TextureArray>()
        .event(flecs::OnAdd)
        .each([=](flecs::entity array, TextureArray &) {
            flecs::world world{array.world()};

            auto &instance_buffer =
                world.entity()
                    .set<Buffer>({wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst})
                    .set<BindingLayout>({instance_layout})
                    .add<Binding>()
                    .set<InstanceBuffer>({});

            world.entity()
                .add<ImageBinding>()
                .add<BoundInstances>(instance_buffer)
                .add<BoundTextures>(array)
                .set<Binds, Uniforms>({0})
                .set<Binds>(instance_buffer, {1})
                .set<Binds>(array, {2});
        });

    // Write vertex buffer
    auto buffer = vertex_buffer.get_mut<Buffer>();
    if (buffer.count == 0) {
        buffer.write_buffer(webgpu, quad_vertices);
    }

    // Register render sytems
    // Collect quad entities
    world.system<InstanceBuffer, TextureArrayIndex, Quad, Position>()
        .kind<RenderSystems::Initialize>()
        .with<ImageBinding>()
        .with<BoundInstances>("$Instances")
        .term_at(0)
        .src("$Instances")
        .with<BoundTextures>("$TextureArray")
        .term_at(1)
        .second("$TextureArray")
        .src("$Texture")
        .with<RenderTexture>("$Texture")
        .src("$Quad")
        .term_at(2)
        .src("$Quad")
        .term_at(3)
        .src("$Quad")
        .each([](InstanceBuffer &instances, TextureArrayIndex &index, Quad &quad, Position &pos) {
            BufferData data{
                quad.corner_radius,
                quad.corner_radius,
                quad.corner_radius,
                quad.corner_radius,
                pos.x,
                pos.y,
                pos.rotation,
                *reinterpret_cast<float *>(&index.index),
                quad.width,
                quad.height,
            };
            instances.buffer.push_back(data);
        });

    // Update buffer
    world.system<WGPU, ImageBinding>().term_at(0).singleton().kind<RenderSystems::Prepare>().each(
        [](flecs::entity entity, WGPU &webgpu, ImageBinding) {
            entity.target<BoundInstances>().get(
                [&](Buffer &buffer, Binding &binding, BindingLayout &layout, InstanceBuffer &data) {
                    buffer.write_buffer(webgpu, data.buffer);
                    if (data.buffer.size() > 0) {
                        buffer.update_bind_group(webgpu, binding, layout);
                        data.buffer.clear();
                    }
                });
        });

    // Run pipeline
    auto render = [=](flecs::world &world, wgpu::RenderPassEncoder pass) {
        auto pipeline_comp = pipeline.get_mut<RenderPipeline>();
        auto vertices = vertex_buffer.get_mut<Buffer>();
        pass.setPipeline(pipeline_comp.pipeline);
        pass.setVertexBuffer(0, vertices.buffer, 0, vertices.count * vertices.item_size);
        world.each<ImageBinding>([&](flecs::entity binding, ImageBinding) {
            auto instance_buffer = binding.target<BoundInstances>().get_mut<Buffer>();
            if (instance_buffer.count > 0) {
                binding.each<Binds>([&](flecs::entity target) {
                    auto index = binding.get<Binds>(target);
                    auto bind_group = target.get<Binding>();
                    pass.setBindGroup((uint32_t)index.index, bind_group.group, 0, nullptr);
                });
                pass.draw(6, (uint32_t)instance_buffer.count, 0, 0);
            }
        });
    };

    pipeline.set<RenderFunction>({render});
}

} // namespace image_pipeline