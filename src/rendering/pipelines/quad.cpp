#include "../../common.hpp"
#include "flecs.h"
#include "pipelines.hpp"
#include <array>
#include <iostream>

using namespace rendering;

namespace quad_pipeline {

struct BufferData {
    std::array<float, 14> data;
    std::array<float, 2> padding;
};

struct InstanceBuffer {
    std::vector<BufferData> data;
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

wgpu::BindGroupLayout init_bind_group_layout(WGPU &webgpu) {
    using namespace wgpu;

    BindGroupLayoutEntry entry;
    entry.binding = 0;
    entry.buffer.type = BufferBindingType::ReadOnlyStorage;
    entry.visibility = ShaderStage::Vertex;

    BindGroupLayoutDescriptor bind_group_layout_desc;
    bind_group_layout_desc.entryCount = 1;
    bind_group_layout_desc.entries = &entry;
    bind_group_layout_desc.label = toWgpuStringView("Quad Instance Bind Group Layout");

    return webgpu.device.createBindGroupLayout(bind_group_layout_desc);
}

struct QuadPipeline {};

struct QuadInstanceBuffer {};

module::module(flecs::world &world) {
    WGPU &webgpu = world.ensure<WGPU>();
    auto vertex_layout = init_vertex_layout();
    auto instance_layout = init_bind_group_layout(webgpu);
    auto &uniform_layout = world.entity<Uniforms>().get<BindingLayout>().layout.value();
    auto &quad_vertex_buffer = world.entity().set<VertexBuffer>({vertex_layout});

    world.singleton<QuadInstanceBuffer>()
        .add<QuadInstanceBuffer>()
        // Cast required for emscripten
        .set<Buffer>(
            {(wgpu::BufferUsage::W)(wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst)})
        .set<BindingLayout>({instance_layout})
        .add<Binding>()
        .set<InstanceBuffer>({});

    auto buffer = quad_vertex_buffer.get_mut<Buffer>();
    buffer.write_buffer(webgpu, quad_vertices);

    world.system<InstanceBuffer, Quad, Position, Color>()
        .term_at(0)
        .src<QuadInstanceBuffer>()
        .kind<RenderSystems::Initialize>()
        .each([](InstanceBuffer &buffer, Quad &quad, Position &pos, Color &color) {
            BufferData data{{
                                color.color[0],
                                color.color[1],
                                color.color[2],
                                color.color[3],
                                quad.corner_radius,
                                quad.corner_radius,
                                quad.corner_radius,
                                quad.corner_radius,
                                pos.x,
                                pos.y,
                                pos.rotation,
                                0.0f,
                                quad.width,
                                quad.height,
                            },
                            {0.0f, 0.0f}};
            buffer.data.push_back(data);
        });

    world.system<InstanceBuffer, Circle, Position, Color>()
        .term_at(0)
        .src<QuadInstanceBuffer>()
        .kind<RenderSystems::Initialize>()
        .each([](InstanceBuffer &buffer, Circle &circle, Position &pos, Color &color) {
            BufferData data{{
                                color.color[0],
                                color.color[1],
                                color.color[2],
                                color.color[3],
                                circle.radius,
                                circle.radius,
                                circle.radius,
                                circle.radius,
                                pos.x,
                                pos.y,
                                pos.rotation,
                                0.0f,
                                circle.radius * 2.0f,
                                circle.radius * 2.0f,
                            },
                            {0.0f, 0.0f}};
            buffer.data.push_back(data);
        });

    // Update buffer
    world.system<WGPU, Buffer, Binding, BindingLayout, InstanceBuffer>()
        .term_at(0)
        .singleton()
        .with<QuadInstanceBuffer>()
        .kind<RenderSystems::Prepare>()
        .each([=](WGPU &webgpu, Buffer &render_buffer, Binding &binding, BindingLayout &layout,
                  InstanceBuffer &data_buffer) {
            render_buffer.write_buffer(webgpu, data_buffer.data);
            if (data_buffer.data.size() > 0) {
                render_buffer.update_bind_group(webgpu, binding, layout);
                data_buffer.data.clear();
            }
        });

    // Run pipeline
    auto render = [=](flecs::world &world, wgpu::RenderPassEncoder pass) {
        auto pipeline = world.entity<QuadPipeline>();
        auto render_pipeline = pipeline.get_mut<RenderPipeline>();
        auto instance_buffer = world.entity<QuadInstanceBuffer>().get_mut<Buffer>();
        auto vertex_buffer = quad_vertex_buffer.get_mut<Buffer>();

        if (instance_buffer.count > 0) {
            pass.setPipeline(render_pipeline.pipeline);
            pass.setVertexBuffer(0, vertex_buffer.buffer, 0,
                                 vertex_buffer.count * vertex_buffer.item_size);
            pipeline.each<Binds>([&](flecs::entity target) {
                auto binding = pipeline.get<Binds>(target);
                auto bind_group = target.get<Binding>();
                pass.setBindGroup((uint32_t)binding.index, bind_group.group, 0, nullptr);
            });
            pass.draw(6, (uint32_t)instance_buffer.count, 0, 0);
        }
    };

    world.singleton<QuadPipeline>()
        .add<PipelineVertices>(quad_vertex_buffer)
        .set<Binds, Uniforms>({0})
        .set<Binds, QuadInstanceBuffer>({1})
        .set<Shader>({ASSET_DIR "/shaders/quad.wgsl"})
        .set<RenderPipeline>({vertex_layout, {uniform_layout, instance_layout}})
        .set<RenderFunction>({render});
}

} // namespace quad_pipeline