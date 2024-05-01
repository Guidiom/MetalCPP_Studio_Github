//
//  AAPLPoints.metal
//  MetalCPP
//
//  Created by Guido Schneider on 10.04.24.
//

#include <metal_stdlib>
using namespace metal;

// Include header shared between this Metal shader code and C code executing Metal API commands
#include "AAPLShaderTypes.h"

// Include header shared between all Metal shader code files
#include "AAPLShaderCommon.h"

struct PointInOut
{
    float4 position [[position]];
    half3  color;
    half2  tex_coord;
};

vertex PointInOut point_vertex(constant SimpleVertex * vertices             [[ buffer(BufferIndexPointVertexData) ]],
                               const device PointLightData * light_data     [[ buffer(BufferIndexLightData) ]],
                               const device vector_float4 * light_positions [[ buffer(BufferIndexLightsPosition) ]],
                               constant FrameData             &frameData    [[ buffer(BufferIndexFrameData) ]],
                               uint                        iid              [[ instance_id ]],
                               uint                        vid              [[ vertex_id ]])
{
    PointInOut out;
    
    const SimpleVertex verts = vertices[vid];
    
    float3 vertex_position = float3(verts.position.xy,0);
    
    float4 point_eye_pos = light_positions[iid];

    float4 vertex_eye_position = float4(frameData.point_size * vertex_position + point_eye_pos.xyz, 1);
    
    out.position = frameData.perspectiveTransform * vertex_eye_position;

    // Pass fairy color through
    out.color = half3(light_data[iid].pointLightColor.xyz);

    // Convert model position which ranges from [-1, 1] to texture coordinates which ranges
    // from [0-1]
    out.tex_coord = 0.5 * (half2(verts.position.xy) + 1);

    return out;
}

fragment half4 point_fragment(PointInOut      in       [[ stage_in ]],
                              texture2d<half> pointMap [[ texture(TextureIndexAlpha) ]])
{
    constexpr sampler linearSampler (mip_filter::linear,
                                     mag_filter::linear,
                                     min_filter::linear);

    half4 c = pointMap.sample(linearSampler, float2(in.tex_coord));

    half3 fragColor = in.color * c.x;

    return half4(fragColor, c.x);
}

