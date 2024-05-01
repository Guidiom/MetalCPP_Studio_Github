//
//  AAPLGround.metal
//  MetalCPP
//
//  Created by Guido Schneider on 21.01.24.
//

#include <metal_stdlib>
using namespace metal;

#include "AAPLShaderTypes.h"
#include "AAPLShaderCommon.h"

struct QuadInOut
{
    float4 position [[position]];
    float3 normal;
    float3 worldPos;
    float2 shadow_uv;
    half shadow_depth;
};

constexpr sampler shadowSampler(coord::normalized,
                                mip_filter::none,
                                address::clamp_to_edge,
                                compare_func:: less);

constant half3 kRec709Luma = half3(0.2126, 0.7152, 0.0722);

vertex QuadInOut ground_vertex(constant GroundVertex * vertices  [[ buffer(BufferIndexGroundVertexData)]],
                               uint vid [[ vertex_id ]],
                               constant FrameData  & frameData    [[ buffer(BufferIndexFrameData) ]])

{
    QuadInOut out;
    constant GroundVertex & gv = vertices[vid];
    
    float4 groundPosition = float4(gv.position.x, 0.f, gv.position.y, 1.f) ;

    out.worldPos = groundPosition.xyz;
    
    out.position = frameData.perspectiveTransform * frameData.planeModelViewMatrix *  groundPosition;
    
    float3 normal = float3(gv.normal.x, 0.f, gv.normal.y);
    out.normal = normalize(frameData.normalPlaneModelViewMatrix * normal);
    
    float4 shadow_coord = frameData.shadow_xform_matrix * frameData.shadow_projections_matrix * frameData.shadow_view_matrix * groundPosition;
    
    out.shadow_uv = shadow_coord.xy;
    out.shadow_depth = half(shadow_coord.z);
    
    return out;
}

fragment GBufferData ground_fragment (QuadInOut      in                  [[ stage_in ]],
                                      constant FrameData  & frameData    [[ buffer(BufferIndexFrameData) ]],
                                      depth2d<float>     shadowMap       [[ texture(TextureIndexShadowMap) ]])
{
    GBufferData gBuffer;
    
    half3 eye_normal = half3(in.normal);
    const int neighborWidth = 3;
    const float neighbors = (neighborWidth * 2.0 + 1.0) *
                            (neighborWidth * 2.0 + 1.0);
    float mapSize = 2048;
    float texelSize = 1.0 / mapSize;
    float total = 0.f;
    for (int x = -neighborWidth; x <= neighborWidth; x++) {
      for (int y = -neighborWidth; y <= neighborWidth; y++) {
          total += shadowMap.sample_compare(shadowSampler, in.shadow_uv + float2(x,y) * texelSize, in.shadow_depth);
        }
      }
    total /= neighbors;
    half shadow_sample = (half)total;
    
    float onEdge;
    float2 onEdge2d = fract(float2(in.worldPos.xz));
    float2 offset2d = sign(onEdge2d) * -0.5 + 0.5;
    onEdge2d.xy += offset2d.xy;
    onEdge2d = step (0.02, onEdge2d);
    onEdge = min( onEdge2d.x, onEdge2d.y);
    
    half3 neutralColor = half3 (0.7, 0.7, 0.7);
    half3 edgeColor = neutralColor * 0.9;
    half3 groundColor = mix (edgeColor, neutralColor, onEdge);
    
    half specular_contrib = dot(groundColor,kRec709Luma);
   
    gBuffer.albedo_specular = half4(groundColor, specular_contrib );
    gBuffer.normal_shadow = half4(eye_normal.xyz, shadow_sample);
    gBuffer.depth = in.worldPos.z;
    
    return gBuffer;
}
