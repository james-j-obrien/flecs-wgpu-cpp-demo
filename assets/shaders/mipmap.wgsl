@group(0) @binding(0) var previous_view: texture_2d_array<f32>;
@group(0) @binding(1) var next_view: texture_storage_2d_array<rgba8unorm,write>;

@group(1) @binding(0) var<uniform> index: u32;

@compute @workgroup_size(8, 8)
fn compute(@builtin(global_invocation_id) id: vec3<u32>) {
    let offset = vec2<u32>(0u, 1u);
    let color = (
        textureLoad(previous_view, 2u * id.xy + offset.xx, index, 0) +
        textureLoad(previous_view, 2u * id.xy + offset.xy, index, 0) +
        textureLoad(previous_view, 2u * id.xy + offset.yx, index, 0) +
        textureLoad(previous_view, 2u * id.xy + offset.yy, index, 0)
    ) * 0.25;
    textureStore(next_view, id.xy, index, color);
}