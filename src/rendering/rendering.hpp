#pragma once

#include "flecs.h"
#include <array>
#include <filesystem>
#include <optional>
#include "webgpu/webgpu.hpp"
#include <GLFW/glfw3.h>
#include "stb_image.h"

namespace rendering {

struct Ready {};

struct Uniforms {
    std::array<float, 2> viewport;
};

struct WGPU {
    wgpu::Adapter adapter = nullptr;
    wgpu::Device device = nullptr;
    wgpu::Instance instance = nullptr;
    wgpu::Queue queue = nullptr;
    wgpu::Color clear_color;

    static void on_remove(WGPU &value) {
        value.queue.release();
        // value.device.release();
        value.instance.release();
    }
};

struct Encoder {
    wgpu::CommandEncoder ptr = nullptr;
};

struct RenderSystems {
    struct Load {};       // Load assets (shaders)
    struct Initialize {}; // Create render resources (pipelines, layouts and buffers)
    struct Prepare {};    // Fill render resources (buffers and bind groups)
    struct Queue {};      // Run pipelines
};

// Tags to associate pipeline with it's resources
struct PipelineVertices {};

struct Binds {
    size_t index;
};

struct PipelineShader {};

// Relation for texture for a rect to render
struct RenderTexture {};

struct ImageData {
    uint32_t width;
    uint32_t height;
    unsigned char *data;

    static void on_remove(ImageData &value) { stbi_image_free(value.data); }
};

struct MainPass {};

struct RenderFunction {
    std::function<void(flecs::world &, wgpu::RenderPassEncoder)> fn;
};

// Render resources
struct VertexBuffer {
    wgpu::VertexBufferLayout layout;

    static void on_remove(VertexBuffer &value) {
        if (value.layout.attributes != nullptr) {
            delete value.layout.attributes;
            value.layout.attributes = nullptr;
        }
    }
};

struct RenderPipeline {
    wgpu::VertexBufferLayout vertex_layout;
    std::vector<wgpu::BindGroupLayout> group_layouts;
    wgpu::RenderPipeline pipeline = nullptr;

    static void on_remove(RenderPipeline &value) {
        if (value.pipeline != nullptr) {
            value.pipeline.release();
        }
        // if (value.vertex_layout.attributes != nullptr) {
        //     delete value.vertex_layout.attributes;
        //     value.vertex_layout.attributes = nullptr;
        // }
        for (auto group : value.group_layouts) {
            group.release();
        }
    }
};

struct ComputePipeline {
    std::vector<wgpu::BindGroupLayout> group_layouts;
    wgpu::ComputePipeline pipeline = nullptr;

    static void on_remove(ComputePipeline &value) {
        if (value.pipeline != nullptr) {
            value.pipeline.release();
        }
        for (auto group : value.group_layouts) {
            group.release();
        }
    }
};

struct Shader {
    std::filesystem::path path;
    wgpu::ShaderModule module = nullptr;

    static void on_remove(Shader &value) {
        if (value.module != nullptr) {
            value.module.release();
        }
    }
};

struct BindingLayout {
    std::optional<wgpu::BindGroupLayout> layout;

    static void on_remove(BindingLayout &value) {
        if (value.layout.has_value()) {
            value.layout.value().release();
        }
    }
};

struct Binding {
    wgpu::BindGroup group = nullptr;

    static void on_remove(Binding &value) {
        if (value.group != nullptr) {
            value.group.release();
        }
    }
};

struct Buffer {
    wgpu::BufferUsage usage;
    size_t count = 0;
    size_t item_size = 0;
    wgpu::Buffer buffer = nullptr;

    static void on_remove(Buffer &value) {
        if (value.buffer != nullptr) {
            value.buffer.destroy();
            value.buffer.release();
        }
    }

    void init_buffer(WGPU &webgpu, uint64_t size) {
        wgpu::BufferDescriptor buffer_desc;
        buffer_desc.usage = usage;
        buffer_desc.mappedAtCreation = false;
        buffer_desc.size = size;
        buffer = webgpu.device.createBuffer(buffer_desc);
    }

    template <typename T> void write_buffer(WGPU &webgpu, T &data) {
        count = 1;
        item_size = sizeof(T);
        auto size = count * item_size;

        if (buffer != nullptr && buffer.getSize() < size) {
            buffer.release();
            init_buffer(webgpu, count * item_size);
        } else if (buffer == nullptr) {
            init_buffer(webgpu, count * item_size);
        }

        webgpu.queue.writeBuffer(buffer, 0, &data, size);
    }

    template <typename T> void write_buffer(WGPU &webgpu, std::vector<T> &data) {
        count = data.size();
        item_size = sizeof(T);
        auto size = count * item_size;

        if (buffer != nullptr && buffer.getSize() < size) {
            buffer.release();
            init_buffer(webgpu, count * item_size);
        } else if (buffer == nullptr) {
            init_buffer(webgpu, count * item_size);
        }

        webgpu.queue.writeBuffer(buffer, 0, data.data(), size);
    }

    void update_bind_group(WGPU &webgpu, Binding &group, BindingLayout &layout) {
        if (group.group != nullptr) {
            group.group.release();
        }

        wgpu::BindGroupEntry entry;
        entry.binding = 0;
        entry.buffer = buffer;
        entry.offset = 0;
        entry.size = count * item_size;

        wgpu::BindGroupDescriptor descriptor;
        descriptor.layout = layout.layout.value();
        descriptor.entryCount = 1;
        descriptor.entries = &entry;
        group.group = webgpu.device.createBindGroup(descriptor);
    }
};

// Relationship: Image -> TextureArray, stores index into the array
struct TextureArrayIndex {
    uint32_t index;
};

struct TextureArray {
    wgpu::Texture texture = nullptr;
    wgpu::Sampler sampler = nullptr;
    wgpu::TextureView view = nullptr;

    std::vector<wgpu::TextureView> mip_views;
    std::vector<wgpu::Extent3D> mip_sizes;

    // wgpu::Extent3D size;
    uint32_t count = 0;

    wgpu::Extent3D &size() { return mip_sizes[0]; }

    static void on_remove(TextureArray &value) {
        value.texture.release();
        value.view.release();
        value.sampler.release();
    }
};

struct TextureArrayKey {
    uint32_t width;
    uint32_t height;
    bool operator==(const TextureArrayKey &other) const {
        return (width == other.width && height == other.height);
    }
    struct Hash {
        std::size_t operator()(const TextureArrayKey &s) const noexcept {
            std::size_t h1 = std::hash<uint32_t>{}(s.width);
            std::size_t h2 = std::hash<uint32_t>{}(s.height);
            return h1 ^ (h2 << 1);
        }
    };
};

struct TextureArrayCache {
    std::unordered_map<TextureArrayKey, flecs::entity_t, TextureArrayKey::Hash> map;
};

#ifndef EMSCRIPTEN
WGPUStringView toWgpuStringView(std::string_view stdStringView);
#else
const char *toWgpuStringView(std::string_view stdStringView);
#endif

struct module {
    module(flecs::world &world);
};

} // namespace rendering
