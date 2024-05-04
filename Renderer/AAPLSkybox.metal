/*
 See LICENSE folder for this sample’s licensing information.
 
 Abstract:
 Metal shaders used to render skybox
 */
#include <metal_stdlib>
using namespace metal;

#include "AAPLShaderTypes.h"

using simd::float3;
using simd::float4;

///-- Per-vertex inputs fed by vertex buffer laid out with MTLVertexDescriptor in Metal API
struct SkyboxVertex
{
     float4 position [[attribute(VertexAttributePosition)]];
     float3 normal   [[attribute(VertexAttributeNormal)]];
};

struct SkyboxInOut
{
     float4 position [[position]];
     float3 texcoord;
};

half4 fog(float3 position, half4 color);

inline half4 fog(float3 position, half4 color) {
  float distance = position.z;
  float density = 0.02;
  float fog = 1.0 - clamp(exp(-density * distance), 0.0, 1.0);
  half4 fogColor = half4(0.5);
  color = mix(color, fogColor, fog);
  return color;
}

vertex SkyboxInOut skybox_vertex(SkyboxVertex       in         [[ stage_in ]],
                                 constant FrameData& frameData [[ buffer(BufferIndexFrameData) ]])
{
    SkyboxInOut out;
    out.position = frameData.perspectiveTransform * frameData.sky_modelview_matrix * in.position;
    ///-- Pass position through as texcoord
    out.texcoord = in.normal;

    return out;
}

fragment half4 skybox_fragment(SkyboxInOut        in             [[ stage_in ]],
                               constant FrameData& frameData [[ buffer(BufferIndexFrameData) ]],
                               texturecube<half> skybox_texture [[ texture(TextureIndexSkyMap) ]])
{
    constexpr sampler linearSampler( mip_filter::linear, mag_filter::linear, min_filter::linear);
    half4 skyColor = skybox_texture.sample(linearSampler, in.texcoord);
    skyColor = fog( frameData.cameraPos, skyColor);
    return skyColor;
}

