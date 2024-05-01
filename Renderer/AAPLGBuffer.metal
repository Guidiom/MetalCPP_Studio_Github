//
//  AAPLGBuffer.metal
//  MetalCPP
//
//  Created by Guido Schneider on 27.10.23.
//
#include <metal_stdlib>
using namespace metal;

#include "AAPLShaderCommon.h"
#include "AAPLShaderTypes.h"


struct ColorInOut
{
    float4 position [[position]];
    float2 texcoord ;
    float3  normal ;
    float2 shadow_uv;
    half   shadow_depth;
    float3 eye_position;
    float3 worldPos;
};

inline float3 computeNormalMap( ColorInOut in, texture2d<float> normalMap);

constexpr sampler shadowSampler(coord::normalized,
                                filter::linear,
                                mip_filter::none,
                                address::clamp_to_edge,
                                compare_func::less);

constexpr sampler linearSampler(mip_filter::linear,
                                mag_filter::linear,
                                min_filter::linear);

float3 computeNormalMap( ColorInOut in, texture2d<float> normalMap) {
    vector_float3 tangentNormal = normalMap.sample( linearSampler, in.texcoord).xyz * 2.0 - 1.0;
    vector_float3 Q1  = dfdx(in.worldPos);
    vector_float3 Q2  = dfdy(in.worldPos);
    vector_float2 st1 = dfdx(in.texcoord );
    vector_float2 st2 = dfdy(in.texcoord );

    vector_float3 N   = normalize(in.normal);
    vector_float3 T  =  normalize(Q1*st2.y - Q2*st1.y);
    vector_float3 B  =  -normalize(cross(N, T));
    float3x3 TBN = float3x3(T, B, N);

    return normalize(TBN * tangentNormal.xyz);
}

vertex ColorInOut gbuffer_vertex(const device VertexData  *vertexData      [[buffer(BufferIndexVertexData)]],
                                 const device InstanceData *instanceData  [[buffer(BufferIndexInstanceData)]],
                                 constant FrameData      &frameData         [[ buffer(BufferIndexFrameData)]],
                                 uint vertexID                              [[vertex_id]],
                                 uint instanceID                            [[ instance_id]])
{
    ColorInOut out;
    device const VertexData&vd = vertexData[ vertexID ];
    float4 pos = float4(vd.position, 1.0);
    pos = instanceData[ instanceID ].instanceTransform * pos;
    float4 eye_position = pos;
    out.position = frameData.perspectiveTransform * frameData.worldTransform * eye_position;
    out.texcoord = vd.texCoord * frameData.textureScale;

    out.eye_position = eye_position.xyz;

    float3 normal = instanceData[ instanceID].instanceNormalTransform * vd.normal;
    normal = frameData.worldNormalTransform * normal;
    out.normal = normalize(normal);

    float4 worldPos = float4(vd.position, 1.0);
    out.worldPos = (instanceData[ instanceID ].instanceTransform * worldPos).xyz;

    float3 shadow_coord = (frameData.shadow_mvp_xform_matrix * pos).xyz;
    out.shadow_uv = shadow_coord.xy;
    out.shadow_depth = half(shadow_coord.z);

    return out;
}

fragment GBufferData gbuffer_fragment( ColorInOut               in           [[ stage_in ]],
                                      constant FrameData      &frameData    [[ buffer(BufferIndexFrameData) ]],
                                      texture2d<float>          basetMap     [[ texture(TextureIndexBaseColor) ]],
                                      texture2d<float>         irridanceMap    [[ texture(TextureIndexIrradianceMap)]],
                                      texture2d<half>          lightMap     [[ texture(TextureIndexTargetLight) ]],
                                      texture2d<float>         normalMap    [[ texture(TextureIndexNormal)]],
                                      depth2d<float>            shadowMap    [[ texture(TextureIndexTargetShadow)]])
{


    GBufferData gBuffer;

    float3 eye_normal = computeNormalMap( in, normalMap );

    half shadow_sample = shadowMap.sample_compare(shadowSampler, in.shadow_uv, in.shadow_depth);
    
    gBuffer.light = half4(half3(eye_normal.xyz), shadow_sample);

    gBuffer.shadow = half4( half3(eye_normal.xyz), shadow_sample);

    return gBuffer;
}


