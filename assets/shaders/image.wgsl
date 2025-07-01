struct Vertex {
	@builtin(instance_index) instance: u32,
	@location(0) pos: vec3<f32>,
}

struct VertexOutput {
	@builtin(position) clip_position: vec4<f32>,
	@location(0) uv: vec2<f32>,
	@location(1) size: vec2<f32>,
	@location(2) corner_radii: vec4<f32>,
    @location(3) texture_uv: vec2<f32>,
    @location(4) @interpolate(flat) index: u32,
}

struct Shape {
	@location(0) corner_radii: vec4<f32>,
	@location(1) position: vec3<f32>,
    @location(2) index: u32,
	@location(3) size: vec2<f32>,
}

struct Uniform {
    viewport: vec2<f32>,
}

@group(0) @binding(0) 
var<uniform> uniforms: Uniform;

@group(1) @binding(0) 
var<storage> shapes: array<Shape>;

@group(2) @binding(0)
var texture: texture_2d_array<f32>;

@group(2) @binding(1)
var texture_sampler: sampler;

const AA_PADDING: f32 = 0.0;

// Given a position, and a size determine the distance between a point and the rectangle with those side lengths
fn rectSDF(position: vec2<f32>, size: vec2<f32>) -> f32 {
    // Rectangles are symmetrical across both axis so we can mirror our point 
    // into the positive x and y axis by taking the absolute value
    var pos = abs(position);

    // Calculate the vector from the corner of the rect to our point
    var to_corner = pos - size;

    // By clamping away negative values we now have the vector to the edge of the rect
    // from outside, however if we are inside the rect this is all 0s
    var outside_to_edge = max(vec2<f32>(0.), to_corner);

    // If the point is inside the rect then it is always below or to the left of our corner 
    // so take the largest negative value from our vector, this will be 0 outside the rect
    var inside_length = min(0., max(to_corner.x, to_corner.y));

    // Combining these two lengths gives us the length for all cases
    return length(outside_to_edge) + inside_length;
}

// Given a uv position get which quadrant that position is in
// Return an integer from 0 to 3
fn quadrant(in: vec2<f32>) -> i32 {
    var uv = vec2<i32>(sign(in));
    return -uv.y + (-uv.x * uv.y + 3) / 2;
}

@vertex
fn vs_main(v: Vertex) -> VertexOutput {
    var out: VertexOutput;

    let vertex = v.pos;
    let shape = shapes[v.instance];

    let shortest_side = min(shape.size.x, shape.size.y);

	// Scale outputs to UV space
    out.size = shape.size / shortest_side;
    out.corner_radii = 2.0 * min(shape.corner_radii / shortest_side, vec4<f32>(0.5));

    let c = cos(-shape.position.z);
    let s = sin(-shape.position.z);
    let scaled_vertex = vertex.xy * shape.size;
    let padded_vertex = scaled_vertex + sign(scaled_vertex) * AA_PADDING;
    let uv_ratio = padded_vertex / scaled_vertex;
    let rotated_vertex = vec2<f32>(
        c * scaled_vertex.x + s * scaled_vertex.y,
        c * scaled_vertex.y - s * scaled_vertex.x
    );

    out.index = shape.index;
    out.uv = vertex.xy * out.size * uv_ratio;
    out.texture_uv = (vertex.xy + 1.0) / 2.0;
    out.clip_position = vec4<f32>((rotated_vertex.xy + shape.position.xy * 2.0) / uniforms.viewport, 0.0, 1.0);

    return out;
}
@ fragment
fn fs_main(f: VertexOutput) -> @ location(0) vec4<f32> {
    let quadrant = quadrant(f.uv);
    let radii = f.corner_radii[quadrant];

    let dist = rectSDF(f.uv, f.size - radii) - radii;
    let in_shape = step(dist, 0.0);

    let tex_uv = vec2<f32>(f.texture_uv.x, 1.0 - f.texture_uv.y);
    let sample = textureSample(texture, texture_sampler, tex_uv, f.index).rgb;
    let color = vec4<f32>(sample, in_shape);

    if in_shape < 0.00001 {
        discard;
    }

    return color;
}