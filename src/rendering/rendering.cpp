#include "../include.hpp"
#include "pipelines/pipelines.hpp"
#include "types/types.hpp"
#include "stb_image.h"
#include <iostream>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.hpp>

using namespace wgpu;

namespace rendering {

WGPUStringView toWgpuStringView(std::string_view stdStringView) {
    return {stdStringView.data(), stdStringView.size()};
}

void device_lost(const WGPUDevice *, WGPUDeviceLostReason reason, WGPUStringView message,
                 void * /* pUserData */, void * /* pUserData */) {
    std::cout << "Device lost: reason " << reason;
    if (message.data) {
        std::cout << " (" << message.data << ")";
    }
    std::cout << std::endl;
}

void device_error(const WGPUDevice *, WGPUErrorType type, WGPUStringView message,
                  void * /* pUserData */, void * /* pUserData */) {
    std::cout << "Uncaptured device error: type " << type;
    if (message.data) {
        std::cout << " (" << message.data << ")";
    }
    std::cout << std::endl;
}

static WGPU init_webgpu(Window &window) {
    // Initialize WebGPU
    InstanceDescriptor desc = {};
    desc.nextInChain = nullptr;

    Instance instance = createInstance(desc);

    if (!instance) {
        std::cerr << "Could not initialize WebGPU" << std::endl;
        return {};
    }

    // or surface.getPreferredFormat()
    TextureFormat format = TextureFormat::BGRA8Unorm;

    Surface surface = glfwCreateWindowWGPUSurface(instance, window.ptr);
    window.surface = surface;

    RequestAdapterOptions adapter_options = {};
    adapter_options.nextInChain = nullptr;
    adapter_options.compatibleSurface = surface;
    Adapter adapter = instance.requestAdapter(adapter_options);

    DeviceLostCallbackInfo device_lost_callback = {};
    device_lost_callback.callback = &device_lost;
    device_lost_callback.mode = WGPUCallbackMode::WGPUCallbackMode_AllowSpontaneous;

    UncapturedErrorCallbackInfo error_callback = {};
    error_callback.callback = &device_error;

    DeviceDescriptor device_desc = {};
    device_desc.label = toWgpuStringView("Primary WGPU Device");
    device_desc.requiredFeatureCount = 0;
    device_desc.requiredLimits = nullptr;
    device_desc.defaultQueue.label = toWgpuStringView("Primary WGPU Queue");
    device_desc.defaultQueue.nextInChain = nullptr;
    device_desc.deviceLostCallbackInfo = device_lost_callback;
    device_desc.uncapturedErrorCallbackInfo = error_callback;
    Device device = adapter.requestDevice(device_desc);
    Queue queue = device.getQueue();

    SurfaceConfiguration config = {};
    config.nextInChain = nullptr;
    config.width = window.width;
    config.height = window.height;
    config.format = format;
    config.viewFormatCount = 0;
    config.viewFormats = nullptr;
    config.usage = TextureUsage::RenderAttachment;
    config.device = device;
    config.presentMode = PresentMode::Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;
    surface.configure(config);

    return {adapter, device, instance, queue, wgpu::Color{0.4, 0.4, 0.4, 1.0}};
}

static BindGroupLayout init_uniform_bind_group_layout(WGPU &webgpu) {
    BindGroupLayoutEntry entry;
    entry.binding = 0;
    entry.buffer.type = BufferBindingType::Uniform;
    entry.visibility = ShaderStage::Vertex;

    BindGroupLayoutDescriptor bind_group_layout_desc;
    bind_group_layout_desc.entryCount = 1;
    bind_group_layout_desc.entries = &entry;
    bind_group_layout_desc.label = toWgpuStringView("Uniform Bind Group Layout");

    return webgpu.device.createBindGroupLayout(bind_group_layout_desc);
}

module::module(flecs::world &world) {
    // Register remove hooks
    world.component<WGPU>().on_remove(&WGPU::on_remove);
    world.component<ImageData>().on_remove(&ImageData::on_remove);
    world.component<VertexBuffer>().on_remove(&VertexBuffer::on_remove);
    world.component<RenderPipeline>().on_remove(&RenderPipeline::on_remove);
    world.component<ComputePipeline>().on_remove(&ComputePipeline::on_remove);
    world.component<Shader>().on_remove(&Shader::on_remove);
    world.component<Buffer>().on_remove(&Buffer::on_remove);
    world.component<BindingLayout>().on_remove(&BindingLayout::on_remove);
    world.component<Binding>().on_remove(&Binding::on_remove);
    world.component<TextureArray>().on_remove(&TextureArray::on_remove);
    world.component<Window>().on_remove(&Window::on_remove);

    world.component<RenderTexture>().add(flecs::Traversable);

    world.entity<RenderSystems::Load>().add(flecs::Phase).depends_on(flecs::OnUpdate);
    world.entity<RenderSystems::Initialize>().add(flecs::Phase).depends_on<RenderSystems::Load>();
    world.entity<RenderSystems::Prepare>()
        .add(flecs::Phase)
        .depends_on<RenderSystems::Initialize>();
    world.entity<RenderSystems::Queue>().add(flecs::Phase).depends_on<RenderSystems::Prepare>();

    world.component<Ready>();
    world.observer<Ready>().event(flecs::OnAdd).each([](flecs::entity e, Ready) {
        e.emit<Ready>();
    });

    world.import <window::module>();

    auto &window = world.ensure<Window>();
    WGPU webgpu_instance = init_webgpu(window);

    if (webgpu_instance.instance == nullptr) {
        std::cerr << "Could not initialize WebGPU!" << std::endl;
        world.quit();
        return;
    }

    world.set<WGPU>(std::move(webgpu_instance));
    world.pipeline<MainPass>().with(flecs::System).with<MainPass>().build();

    world.import <types::module>();

    auto &webgpu = world.ensure<WGPU>();

    auto layout = init_uniform_bind_group_layout(webgpu);
    world.component<Uniforms>()
        .set<Buffer>({BufferUsage::Uniform | BufferUsage::CopyDst})
        .set<BindingLayout>({layout})
        .set<Binding>({});
    world.set<Uniforms>({{(float)window.width, (float)window.height}});
    world.set<Encoder>({});

    world.import <pipelines::module>();

    // Instantiate command encoder
    world.system<WGPU>().kind<RenderSystems::Initialize>().each([](flecs::entity e, WGPU &webgpu) {
        CommandEncoderDescriptor command_encoder_desc;
        command_encoder_desc.label = toWgpuStringView("Command Encoder");
        CommandEncoder encoder = webgpu.device.createCommandEncoder(command_encoder_desc);

        e.world().set<Encoder>({encoder});
    });

    // Prepare uniforms
    world.system<WGPU>().kind<RenderSystems::Prepare>().each([](flecs::entity e, WGPU &webgpu) {
        e.world().entity<Uniforms>().get(
            [&](Buffer &buffer, BindingLayout &layout, Binding &binding, Uniforms &data) {
                buffer.write_buffer(webgpu, data);
                buffer.update_bind_group(webgpu, binding, layout);
            });
    });

    // Render main pass
    world.system<WGPU, Encoder, Window>()
        .term_at(0)
        .singleton()
        .term_at(1)
        .singleton()
        .kind<RenderSystems::Queue>()
        .each([](flecs::entity e, WGPU &webgpu, Encoder &encoder, Window &window) {
            SurfaceTexture surface_texture;
            window.surface.getCurrentTexture(&surface_texture);

            if (surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
                return;
            }

            flecs::world world{e.world()};

            TextureViewDescriptor view_desc;
            view_desc.nextInChain = nullptr;
            view_desc.label = toWgpuStringView("Surface texture view");
            view_desc.format = TextureFormat::BGRA8Unorm;
            view_desc.dimension = WGPUTextureViewDimension_2D;
            view_desc.baseMipLevel = 0;
            view_desc.mipLevelCount = 1;
            view_desc.baseArrayLayer = 0;
            view_desc.arrayLayerCount = 1;
            view_desc.aspect = WGPUTextureAspect_All;
            TextureView texture_view = wgpuTextureCreateView(surface_texture.texture, &view_desc);

            RenderPassDescriptor render_pass_desc;

            RenderPassColorAttachment render_pass_color_attachment;
            render_pass_color_attachment.view = texture_view;
            render_pass_color_attachment.resolveTarget = nullptr;
            render_pass_color_attachment.loadOp = LoadOp::Clear;
            render_pass_color_attachment.storeOp = StoreOp::Store;
            render_pass_color_attachment.clearValue = webgpu.clear_color;
            render_pass_desc.colorAttachmentCount = 1;
            render_pass_desc.colorAttachments = &render_pass_color_attachment;

            render_pass_desc.depthStencilAttachment = nullptr;
            render_pass_desc.timestampWrites = nullptr;
            RenderPassEncoder render_pass = encoder.ptr.beginRenderPass(render_pass_desc);

            world.each([=](flecs::entity e, RenderFunction &func) {
                flecs::world world{e.world()};
                func.fn(world, render_pass);
            });

            render_pass.end();
            render_pass.release();
            texture_view.release();

            CommandBufferDescriptor command_buffer_descriptor;
            command_buffer_descriptor.label = toWgpuStringView("Command buffer");
            CommandBuffer command = encoder.ptr.finish(command_buffer_descriptor);
            webgpu.queue.submit(command);
            encoder.ptr.release();

#ifndef __EMSCRIPTEN__
            window.surface.present();
#endif
            command.release();

#ifdef WEBGPU_BACKEND_DAWN
            // Check for pending error callbacks
            webgpu.device.tick();
#endif
        });
}
} // namespace rendering
