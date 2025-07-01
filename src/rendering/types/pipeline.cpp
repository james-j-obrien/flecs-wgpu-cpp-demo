#include "flecs.h"
#include "types.hpp"
#include <iostream>
#include <webgpu/webgpu.hpp>
#include <filesystem>
#include <fstream>
#include "../../common.hpp"

using namespace rendering;

namespace pipeline {

void init_render_pipeline(WGPU &webgpu, flecs::entity e, RenderPipeline &pipeline) {
    using namespace wgpu;

    ShaderModule &shader_module = e.get_mut<Shader>().module;

    // or surface.getPreferredFormat()
    TextureFormat format = TextureFormat::BGRA8Unorm;

    RenderPipelineDescriptor pipeline_desc;

    pipeline_desc.vertex.bufferCount = 1;
    pipeline_desc.vertex.buffers = &pipeline.vertex_layout;

    pipeline_desc.vertex.module = shader_module;
    pipeline_desc.vertex.entryPoint = toWgpuStringView("vs_main");
    pipeline_desc.vertex.constantCount = 0;
    pipeline_desc.vertex.constants = nullptr;

    pipeline_desc.primitive.topology = PrimitiveTopology::TriangleList;
    pipeline_desc.primitive.stripIndexFormat = IndexFormat::Undefined;
    pipeline_desc.primitive.frontFace = FrontFace::CCW;
    pipeline_desc.primitive.cullMode = CullMode::None;

    FragmentState fragment_state;
    fragment_state.module = shader_module;
    fragment_state.entryPoint = toWgpuStringView("fs_main");
    fragment_state.constantCount = 0;
    fragment_state.constants = nullptr;

    BlendState blendState;
    // Usual alpha blending for the color:
    blendState.color.srcFactor = BlendFactor::SrcAlpha;
    blendState.color.dstFactor = BlendFactor::OneMinusSrcAlpha;
    blendState.color.operation = BlendOperation::Add;
    // We leave the target alpha untouched:
    blendState.alpha.srcFactor = BlendFactor::Zero;
    blendState.alpha.dstFactor = BlendFactor::One;
    blendState.alpha.operation = BlendOperation::Add;

    ColorTargetState colorTarget;
    colorTarget.format = format;
    colorTarget.blend = &blendState;
    colorTarget.writeMask = ColorWriteMask::All;

    fragment_state.targetCount = 1;
    fragment_state.targets = &colorTarget;

    pipeline_desc.fragment = &fragment_state;

    pipeline_desc.depthStencil = nullptr;

    pipeline_desc.multisample.count = 1;
    pipeline_desc.multisample.mask = ~0u;
    pipeline_desc.multisample.alphaToCoverageEnabled = false;

    PipelineLayoutDescriptor pipeline_layout_desc;
    pipeline_layout_desc.bindGroupLayoutCount = pipeline.group_layouts.size();
    pipeline_layout_desc.bindGroupLayouts = (WGPUBindGroupLayout *)pipeline.group_layouts.data();

    pipeline_desc.layout = webgpu.device.createPipelineLayout(pipeline_layout_desc);

    pipeline.pipeline = webgpu.device.createRenderPipeline(pipeline_desc);
}

void init_compute_pipeline(WGPU &webgpu, flecs::entity e, ComputePipeline &pipeline) {
    using namespace wgpu;

    ShaderModule &shader_module = e.get_mut<Shader>().module;

    ComputePipelineDescriptor pipeline_desc;
    pipeline_desc.compute.constantCount = 0;
    pipeline_desc.compute.constants = nullptr;
    pipeline_desc.compute.entryPoint = toWgpuStringView("compute");
    pipeline_desc.compute.module = shader_module;

    PipelineLayoutDescriptor pipeline_layout_desc;
    pipeline_layout_desc.bindGroupLayoutCount = pipeline.group_layouts.size();
    pipeline_layout_desc.bindGroupLayouts = (WGPUBindGroupLayout *)pipeline.group_layouts.data();

    pipeline_desc.layout = webgpu.device.createPipelineLayout(pipeline_layout_desc);

    pipeline.pipeline = webgpu.device.createComputePipeline(pipeline_desc);
}

wgpu::ShaderModule load_shader(WGPU &webgpu, const std::filesystem::path &path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Could not load shader." << std::endl;
        return nullptr;
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string src(size, ' ');
    file.seekg(0);
    file.read(src.data(), size);

#ifndef EMSCRIPTEN
    wgpu::ShaderSourceWGSL shader_code_desc{};
    shader_code_desc.chain.next = nullptr;
    shader_code_desc.chain.sType = wgpu::SType::ShaderSourceWGSL;
    shader_code_desc.code = toWgpuStringView(src);
#else
    wgpu::ShaderModuleWGSLDescriptor shader_code_desc{};
    shader_code_desc.chain.next = nullptr;
    shader_code_desc.chain.sType = wgpu::SType::ShaderModuleWGSLDescriptor;
    shader_code_desc.code = toWgpuStringView(src);
#endif

    wgpu::ShaderModuleDescriptor shader_desc;
    shader_desc.nextInChain = &shader_code_desc.chain;

    return webgpu.device.createShaderModule(shader_desc);
}

module::module(flecs::world &world) {
    world.observer<WGPU, RenderPipeline, Shader>()
        .event(flecs::OnSet)
        .term_at(0)
        .singleton()
        .without<Ready>()
        .each([](flecs::entity e, WGPU &webgpu, RenderPipeline &pipeline, Shader &shader) {
            shader.module = load_shader(webgpu, shader.path);
            std::cout << "Loaded shader: " << shader.path << std::endl;

            init_render_pipeline(webgpu, e, pipeline);
            e.add<Ready>();
        });

    world.observer<WGPU, ComputePipeline, Shader>()
        .event(flecs::OnSet)
        .term_at(0)
        .singleton()
        .without<Ready>()
        .each([](flecs::entity e, WGPU &webgpu, ComputePipeline &pipeline, Shader &shader) {
            shader.module = load_shader(webgpu, shader.path);
            std::cout << "Loaded compute shader: " << shader.path << std::endl;

            init_compute_pipeline(webgpu, e, pipeline);
            e.add<Ready>();
        });
}
} // namespace pipeline
