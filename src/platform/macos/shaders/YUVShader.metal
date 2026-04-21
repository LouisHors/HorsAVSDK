#include <metal_stdlib>
using namespace metal;

// Vertex input structure
struct VertexIn {
    float2 position [[attribute(0)]];
    float2 texCoord [[attribute(1)]];
};

// Vertex output / Fragment input
struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

// Vertex shader
vertex VertexOut vertexShader(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.position = float4(in.position, 0.0, 1.0);
    out.texCoord = in.texCoord;
    return out;
}

// Fragment shader for YUV to RGB conversion
fragment float4 fragmentShader(VertexOut in [[stage_in]],
                               texture2d<float> yTexture [[texture(0)]],
                               texture2d<float> uTexture [[texture(1)]],
                               texture2d<float> vTexture [[texture(2)]]) {
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);

    float y = yTexture.sample(textureSampler, in.texCoord).r;
    float u = uTexture.sample(textureSampler, in.texCoord).r - 0.5;
    float v = vTexture.sample(textureSampler, in.texCoord).r - 0.5;

    // YUV to RGB conversion (BT.601)
    float r = y + 1.402 * v;
    float g = y - 0.344136 * u - 0.714136 * v;
    float b = y + 1.772 * u;

    return float4(r, g, b, 1.0);
}

// Fragment shader for NV12 (biplanar) format
fragment float4 fragmentShaderNV12(VertexOut in [[stage_in]],
                                   texture2d<float> yTexture [[texture(0)]],
                                   texture2d<float> uvTexture [[texture(1)]]) {
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);

    float y = yTexture.sample(textureSampler, in.texCoord).r;
    float2 uv = uvTexture.sample(textureSampler, in.texCoord).rg - float2(0.5);

    // YUV to RGB conversion (BT.601)
    float r = y + 1.402 * uv.y;
    float g = y - 0.344136 * uv.x - 0.714136 * uv.y;
    float b = y + 1.772 * uv.x;

    return float4(r, g, b, 1.0);
}
