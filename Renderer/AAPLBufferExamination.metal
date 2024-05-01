/*
See LICENSE folder for this sample’s licensing information.

Abstract:
Metal shaders used to render buffer examination mode
*/
#include <metal_stdlib>

using namespace metal;

#include "AAPLShaderTypes.h"
#include "AAPLShaderCommon.h"

struct RenderTextureData
{
    float4 position [[position]];
    float2 tex_coord;
};

vertex RenderTextureData
texture_values_vertex(constant SimpleVertex * vertices  [[ buffer(BufferIndexQuadVertexData) ]],
                      uint                      vid     [[ vertex_id ]])
{
    RenderTextureData out;

    out.position = float4(vertices[vid].position, 0, 1);
    out.tex_coord = (out.position.xy + 1) * .5;
    out.tex_coord.y = 1-out.tex_coord.y;
    return out;
}

fragment half4
texture_rgb_fragment(RenderTextureData in      [[ stage_in ]],
                     texture2d<half>  lighting [[texture(RenderTargetLighting)]]
                     )
{
    constexpr sampler linearSampler(mip_filter::none,
                                    mag_filter::linear,
                                    min_filter::linear);

    half4 sample = lighting.sample(linearSampler, in.tex_coord);
    sample = sample * 0.99;
    return sample;
}

