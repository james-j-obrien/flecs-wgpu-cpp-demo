#include <webgpu/webgpu.hpp>
#include "../../include.hpp"
#include "../stb_image.h"
#include "types.hpp"

using namespace rendering;

namespace image {

struct MipmapPipeline {};

struct MipmapUniforms {};

wgpu::Texture create_texture(WGPU &webgpu, wgpu::Extent3D size, uint32_t mips) {
    using namespace wgpu;

    TextureDescriptor texture_desc;
    texture_desc.dimension = TextureDimension::_2D;
    texture_desc.size = size;
    texture_desc.mipLevelCount = mips;
    texture_desc.sampleCount = 1;
    texture_desc.format = TextureFormat::RGBA8Unorm;
    texture_desc.usage = TextureUsage::TextureBinding | TextureUsage::StorageBinding |
                         TextureUsage::CopyDst | TextureUsage::CopySrc;
    texture_desc.viewFormatCount = 0;
    texture_desc.viewFormats = nullptr;

    return webgpu.device.createTexture(texture_desc);
}

wgpu::TextureView create_texture_view(wgpu::Texture &texture, uint32_t mips = 0,
                                      uint32_t base_mip = 0) {
    using namespace wgpu;

    TextureViewDescriptor texture_view_desc;
    texture_view_desc.aspect = TextureAspect::All;
    texture_view_desc.baseArrayLayer = 0;
    texture_view_desc.arrayLayerCount = texture.getDepthOrArrayLayers();
    texture_view_desc.baseMipLevel = base_mip;
    texture_view_desc.mipLevelCount = mips ? mips : texture.getMipLevelCount();
    texture_view_desc.dimension = TextureViewDimension::_2DArray;
    texture_view_desc.format = TextureFormat::RGBA8Unorm;

    return texture.createView(texture_view_desc);
}

wgpu::Sampler create_sampler(WGPU &webgpu, uint32_t mips) {
    using namespace wgpu;

    SamplerDescriptor sampler_desc;
    sampler_desc.addressModeU = AddressMode::ClampToEdge;
    sampler_desc.addressModeV = AddressMode::ClampToEdge;
    sampler_desc.addressModeW = AddressMode::ClampToEdge;
    sampler_desc.magFilter = FilterMode::Linear;
    sampler_desc.minFilter = FilterMode::Linear;
    sampler_desc.mipmapFilter = MipmapFilterMode::Linear;
    sampler_desc.lodMinClamp = 0.0f;
    sampler_desc.lodMaxClamp = (float)mips;
    sampler_desc.compare = CompareFunction::Undefined;
    sampler_desc.maxAnisotropy = 1;
    return webgpu.device.createSampler(sampler_desc);
}

void recreate_bind_groups(flecs::entity entity, WGPU &webgpu, TextureArray &array) {
    using namespace wgpu;

    auto &layout = entity.world().entity<TextureArrayCache>().ensure<BindingLayout>();
    auto &bind_group = entity.ensure<Binding>();

    if (bind_group.group != nullptr) {
        bind_group.group.release();
    }

    std::vector<BindGroupEntry> entries(2);

    BindGroupEntry &texture_entry = entries[0];
    texture_entry.binding = 0;
    texture_entry.textureView = array.view;

    BindGroupEntry &sampler_entry = entries[1];
    sampler_entry.binding = 1;
    sampler_entry.sampler = array.sampler;

    BindGroupDescriptor bind_group_desc;
    bind_group_desc.layout = layout.layout.value();
    bind_group_desc.entryCount = entries.size();
    bind_group_desc.entries = entries.data();

    bind_group.group = webgpu.device.createBindGroup(bind_group_desc);
}

uint32_t bit_width(uint32_t m) {
    if (m == 0)
        return 0;
    else {
        uint32_t w = 0;
        while (m >>= 1)
            ++w;
        return w;
    }
}

uint32_t max_mip_levels(const wgpu::Extent3D &textureSize) {
    return bit_width(std::max(textureSize.width, textureSize.height));
}

void create_mipmaps(TextureArray &array, wgpu::Extent3D &size, uint32_t mips) {
    for (auto view : array.mip_views) {
        view.release();
    }

    array.mip_sizes.clear();
    array.mip_views.clear();
    array.mip_sizes.resize(mips);
    array.mip_views.reserve(mips);

    array.mip_sizes[0] = size;
    for (size_t level = 0; level < mips; level++) {
        array.mip_views.push_back(create_texture_view(array.texture, 1, (uint32_t)level));
        if (level) {
            auto previous = array.mip_sizes[level - 1];
            array.mip_sizes[level] = {previous.width / 2, previous.height / 2,
                                      previous.depthOrArrayLayers};
        }
    }
}

wgpu::BindGroup init_mipmap_binding(WGPU &webgpu, TextureArray &array, uint32_t level,
                                    const wgpu::BindGroupLayout &layout) {
    using namespace wgpu;

    std::vector<BindGroupEntry> entries(2, Default);

    entries[0].binding = 0;
    entries[0].textureView = array.mip_views[level - 1];

    entries[1].binding = 1;
    entries[1].textureView = array.mip_views[level];

    BindGroupDescriptor desc;
    desc.layout = layout;
    desc.entryCount = (uint32_t)entries.size();
    desc.entries = (WGPUBindGroupEntry *)entries.data();
    return webgpu.device.createBindGroup(desc);
}

void write_mipmaps(flecs::entity entity, WGPU &webgpu, TextureArray &array, uint32_t index) {
    using namespace wgpu;

    flecs::world world{entity.world()};

    CommandEncoderDescriptor command_encoder_desc;
    command_encoder_desc.label = toWgpuStringView("Mipmap Encoder");
    auto encoder = webgpu.device.createCommandEncoder(command_encoder_desc);

    ComputePassDescriptor pass_desc;
    pass_desc.timestampWrites = nullptr;
    auto pass = encoder.beginComputePass(pass_desc);

    auto mipmap_pipeline = world.entity<MipmapPipeline>();
    auto pipeline = mipmap_pipeline.get<rendering::ComputePipeline>();

    auto mipmap_uniforms = world.entity<MipmapUniforms>();
    auto uniform_buffer = mipmap_uniforms.get_mut<rendering::Buffer>();
    auto uniform_binding = mipmap_uniforms.get_mut<rendering::Binding>();

    uniform_buffer.write_buffer(webgpu, index);
    pass.setPipeline(pipeline.pipeline);

    for (uint32_t level = 1; level < array.mip_sizes.size(); level++) {
        auto size = array.mip_sizes[level];

        auto binding = init_mipmap_binding(webgpu, array, level, pipeline.group_layouts[0]);
        pass.setBindGroup(0, binding, 0, nullptr);
        pass.setBindGroup(1, uniform_binding.group, 0, nullptr);

        auto workgroup_size = 8;
        auto workgroup_x = (size.width + workgroup_size - 1) / workgroup_size;
        auto workgroup_y = (size.height + workgroup_size - 1) / workgroup_size;
        pass.dispatchWorkgroups(workgroup_x, workgroup_y, 1);

        binding.release();
    }

    pass.end();

    CommandBufferDescriptor command_buffer_descriptor;
    command_buffer_descriptor.label = toWgpuStringView("Mipmap Command Buffer");
    auto command = encoder.finish(command_buffer_descriptor);
    webgpu.queue.submit(command);
    encoder.release();
}

uint32_t upload_image(flecs::entity entity, ImageData &image) {
    using namespace wgpu;

    flecs::world world{entity.world()};

    auto &array = entity.ensure<TextureArray>();
    auto &webgpu = world.ensure<WGPU>();

    // Create texture~
    if (array.texture == nullptr) {
        wgpu::Extent3D size{image.width, image.height, 1};
        auto mips = max_mip_levels(size);
        array.texture = create_texture(webgpu, size, mips);
        create_mipmaps(array, size, mips);

        array.view = create_texture_view(array.texture);
        array.sampler = create_sampler(webgpu, mips);

        recreate_bind_groups(entity, webgpu, array);
    }

    auto width = array.size().width;
    auto height = array.size().height;
    auto depth = array.size().depthOrArrayLayers;

    assert(width == image.width);
    assert(height == image.height);

    // Re-allocate
    if (depth == array.count) {
        auto mips = (uint32_t)array.mip_sizes.size();
        wgpu::Extent3D new_size = {width, height, depth * 2};
        auto new_texture = create_texture(webgpu, new_size, mips);

        TexelCopyTextureInfo source;
        source.texture = array.texture;
        source.origin = {0, 0, 0};
        source.aspect = TextureAspect::All;

        TexelCopyTextureInfo destination;
        destination.texture = new_texture;
        destination.origin = {0, 0, 0};
        destination.aspect = TextureAspect::All;

        CommandEncoderDescriptor command_encoder_desc;
        command_encoder_desc.label = toWgpuStringView("Texture Copy Encoder");
        auto encoder = webgpu.device.createCommandEncoder(command_encoder_desc);

        for (uint32_t level = 0; level < mips; level++) {
            source.mipLevel = level;
            destination.mipLevel = level;

            Extent3D size{array.mip_sizes[level]};
            size.depthOrArrayLayers = depth;
            encoder.copyTextureToTexture(source, destination, size);
        }

        CommandBufferDescriptor command_buffer_descriptor;
        command_buffer_descriptor.label = toWgpuStringView("Texture Copy Command Buffer");
        auto command = encoder.finish(command_buffer_descriptor);
        webgpu.queue.submit(command);
        encoder.release();

        array.texture.release();
        array.texture = new_texture;
        array.view = create_texture_view(array.texture);
        create_mipmaps(array, new_size, mips);

        recreate_bind_groups(entity, webgpu, array);
    }

    // Write new image
    TexelCopyTextureInfo destination;
    destination.texture = array.texture;
    destination.mipLevel = 0;
    destination.origin = {0, 0, array.count};
    destination.aspect = TextureAspect::All;

    TexelCopyBufferLayout source;
    source.offset = 0;
    source.bytesPerRow = 4 * width;
    source.rowsPerImage = height;

    webgpu.queue.writeTexture(destination, image.data, 4 * width * height, source,
                              {width, height, 1});

    auto index = array.count++;
    write_mipmaps(entity, webgpu, array, index);

    return index;
}

wgpu::BindGroupLayout init_image_bind_group_layout(WGPU &webgpu) {
    using namespace wgpu;

    std::vector<BindGroupLayoutEntry> layout_entries(2, Default);

    BindGroupLayoutEntry &texture_entry = layout_entries[0];
    texture_entry.binding = 0;
    texture_entry.visibility = ShaderStage::Fragment;
    texture_entry.texture.sampleType = TextureSampleType::Float;
    texture_entry.texture.viewDimension = TextureViewDimension::_2DArray;

    BindGroupLayoutEntry &sampler_entry = layout_entries[1];
    sampler_entry.binding = 1;
    sampler_entry.visibility = ShaderStage::Fragment;
    sampler_entry.sampler.type = SamplerBindingType::Filtering;

    BindGroupLayoutDescriptor layout_desc{};
    layout_desc.entryCount = layout_entries.size();
    layout_desc.entries = layout_entries.data();

    return webgpu.device.createBindGroupLayout(layout_desc);
}

wgpu::BindGroupLayout init_mipmap_view_bind_group_layout(WGPU &webgpu) {
    using namespace wgpu;

    std::vector<BindGroupLayoutEntry> bindings(2, Default);

    bindings[0].binding = 0;
    bindings[0].texture.sampleType = TextureSampleType::Float;
    bindings[0].texture.viewDimension = TextureViewDimension::_2DArray;
    bindings[0].visibility = ShaderStage::Compute;

    bindings[1].binding = 1;
    bindings[1].storageTexture.access = StorageTextureAccess::WriteOnly;
    bindings[1].storageTexture.format = TextureFormat::RGBA8Unorm;
    bindings[1].storageTexture.viewDimension = TextureViewDimension::_2DArray;
    bindings[1].visibility = ShaderStage::Compute;

    BindGroupLayoutDescriptor layout_desc;
    layout_desc.entryCount = (uint32_t)bindings.size();
    layout_desc.entries = bindings.data();
    return webgpu.device.createBindGroupLayout(layout_desc);
}

wgpu::BindGroupLayout init_mipmap_uniform_bind_group_layout(WGPU &webgpu) {
    using namespace wgpu;

    BindGroupLayoutEntry binding = Default;
    binding.binding = 0;
    binding.buffer.type = BufferBindingType::Uniform;
    binding.visibility = ShaderStage::Compute;

    BindGroupLayoutDescriptor layout_desc;
    layout_desc.entryCount = 1;
    layout_desc.entries = &binding;
    return webgpu.device.createBindGroupLayout(layout_desc);
}

module::module(flecs::world &world) {
    auto &webgpu = world.ensure<WGPU>();
    auto image_layout = init_image_bind_group_layout(webgpu);
    auto view_layout = init_mipmap_view_bind_group_layout(webgpu);
    auto uniform_layout = init_mipmap_uniform_bind_group_layout(webgpu);

    world.set<TextureArrayCache>({});
    world.entity<TextureArrayCache>().set<TextureArrayCache>({}).set<BindingLayout>({image_layout});

    auto mipmap_uniforms =
        world.entity<MipmapUniforms>()
            .set<Buffer>(
                {wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst, 1, sizeof(uint32_t)})
            .set<BindingLayout>({uniform_layout})
            .set<Binding>({});

    mipmap_uniforms.get([&](Buffer &buffer, Binding &binding, BindingLayout &layout) {
        uint32_t index = 0;
        buffer.write_buffer(webgpu, index);
        buffer.update_bind_group(webgpu, binding, layout);
    });

    auto mipmap_pipeline = world.entity<MipmapPipeline>()
                               .set<Shader>({ASSET_DIR "/shaders/mipmap.wgsl"})
                               .set<ComputePipeline>({{view_layout, uniform_layout}});

    world.observer<TextureArrayCache, ImageData>()
        .term_at(0)
        .singleton()
        .without<TextureArrayIndex>(flecs::Wildcard)
        .event(flecs::OnSet)
        .each([=](flecs::entity e, TextureArrayCache &cache, ImageData &image) {
            flecs::world world{e.world()};

            TextureArrayKey key{image.width, image.height};
            flecs::entity array;
            if (!cache.map.count(key)) {
                array = world.entity();
                cache.map[key] = array;
            } else {
                array = world.entity(cache.map[key]);
            }
            auto index = upload_image(array, image);
            e.set<TextureArrayIndex>(array, {index});
        });
}
} // namespace image