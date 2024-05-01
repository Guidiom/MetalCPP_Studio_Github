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
                               texturecube<half> skybox_texture [[ texture(TextureIndexSkyMap) ]])
{
    constexpr sampler linearSampler( mip_filter::linear, mag_filter::linear, min_filter::linear);
    half4 skyColor = skybox_texture.sample(linearSampler, in.texcoord);

    return skyColor;
}

