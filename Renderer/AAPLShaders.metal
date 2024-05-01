///
///  AAPLShaders.metal
///  MetalCCP
///
///  Created by Guido Schneider on 29.07.22.

#include <metal_stdlib>
using namespace metal;


#ifndef __METAL_VERSION__
/// 96-bit / 12 byte 3 component float vector type
typedef struct __attribute__ ((packed)) packed_float3 {
    float x;
    float y;
    float z;
}packed_float3;
#endif

/// Include header shared between this Metal shader code and C code executing Metal API commands
#include "AAPLShaderTypes.h"

/// Include header shared between all Metal shader code files
#include "AAPLShaderCommon.h"

struct v1f
{
    float4 position [[position]];
    float2 texcoord;
    float3 normal;
    float3 worldPos;
    half3  color;
    float4 shadowPosition;
    float2 shadow_uv;
    half   shadow_depth;
    float  metallnessBias;
    float  colorMixBias;
    float  roughnessBias;
};

struct PBRParameter
{
    float3 textureScale;
    float3 viewDir;
    float3 reflectedVector;
    float3 normal;
    float3 irradiatedColor;
    float3 prefilteredColor;
    float3 baseColor;
    float4 brdfColor;
    float3 sunPosition;
    float4 sunEyeDirection;
    float3 sunColor;
    float3 worldPos;
    float  nDotv;
    float  metalness;
    float  physarum;
    float  roughness;
    float  ambientOcclusion;
    float  colorMixBias;
};

constexpr sampler repeatSampler ( address::repeat, min_filter::linear, mag_filter::linear, mip_filter::linear);
// constexpr sampler nearestSampler(address::repeat,min_filter::nearest,mag_filter::nearest,mip_filter::none);

constexpr sampler linearSampler (min_filter::linear, mag_filter::linear, mip_filter::linear);
constexpr sampler irradiatedSampler (s_address::clamp_to_edge,t_address::clamp_to_edge,r_address::clamp_to_edge , min_filter::linear, mag_filter::linear);
constexpr sampler preFilterSampler (s_address::clamp_to_edge,t_address::clamp_to_edge,r_address::clamp_to_edge , min_filter::linear, mag_filter::linear, mip_filter::linear, lod_clamp(0.f, 4.f));
constexpr sampler bdrfSampler ( s_address::clamp_to_edge, t_address::clamp_to_edge , min_filter::linear, mag_filter::linear );
constexpr sampler shadowSampler(coord::normalized,
                                filter::linear,
                                mip_filter::none,
                                address::clamp_to_edge,
                                compare_func::less);

constant float kMaxHDRValue  = float(500.0f);
//constant float3 kRec709Luma = float3(0.2126f, 0.7152f, 0.0722f);

vector_float3 computeNormalMap(v1f in, texture2d<float> normalMapTexture);
float3 fresnelSchlick(float cosTheta, float3 F0);
float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness);
float  DistributionGGX(float3 N, float3 H, float roughness);
float  GeometrySchlickGGX(float NdotV, float roughness);
float  GeometrySmith(float3 N, float3 V, float3 L, float roughness);
float4 computeSpecular(PBRParameter parameters);
float2 calculateScreenCoord( float3 ndcpos )
{
    float2 screenTexcoord = (ndcpos.xy) * 0.5 + float2(0.5);
    screenTexcoord.y = 1.0 - screenTexcoord.y;
    return screenTexcoord;
}


///--Texture bitangent & tangent mapping-------------------------------------------------------------------------------------------------------------
inline vector_float3 computeNormalMap( v1f in, texture2d<float> normalMapTexture) {
    vector_float3 tangentNormal = normalMapTexture.sample( linearSampler, in.texcoord).xyz * 2.0 - 1.0;
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

///--Fresnel effects--------------------------------------------------------------------------------------------------------------
inline float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

inline float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

///--DistributionGGX-------------------------------------------------------------------------------------------------------------
inline float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return nom / denom;
}

/// --GeometrySchlickGGX------------------------------------------------------------------------------------------------------
inline float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return nom / denom;
}

/// --GeometrySmith------------------------------------------------------------------------------------------------------------
inline float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

/// --computeSpecular------------------------------------------------------------------------------------------------------------
inline float4 computeSpecular(PBRParameter parameters){

    float3 N  = parameters.normal;
    float3 V  = parameters.viewDir;

    float3  F0 = float3(0.04f);
    
    F0 = mix( F0, parameters.baseColor, parameters.metalness);
    float3  Lo = float3(0.f);
    for( int8_t i = 0; i < 1 ; i++)
    {
        float3  L = normalize(parameters.sunEyeDirection.xyz);
        float3  H = normalize(V + L);
        float   distance = length(parameters.sunPosition - parameters.worldPos);
        float   attenuation = 1.f / (distance * distance);
        float3  radiance = parameters.sunColor.xyz * attenuation;

        float NDF = DistributionGGX(N, H, parameters.roughness);
        float   G = GeometrySmith(N, V, L, parameters.roughness);
        float3  F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float3  numerator    = NDF * G * F;
        float   denominator = 4 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        float3  specular = numerator / denominator;
        
        float3  kS = F;

        float3  kD = float3(1.0) - kS;

        kD *= 1.0 - parameters.metalness;

        float NdotL = max(dot(N, L), 0.0);

        Lo += (kD * parameters.baseColor / PI + specular) * radiance * NdotL;
    }

    float3 F = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, parameters.roughness);

    float3 kS  = F;
    float3 kD  = 1.0 - kS;
    
    kD *= 1.0 - parameters.metalness;

    float3  irradiance = parameters.irradiatedColor;
    
    float3  diffuse  =  irradiance * parameters.baseColor;

    float3  specularIBL = parameters.prefilteredColor * (F0 * parameters.brdfColor.x + parameters.brdfColor.y);

    float3 ambient = (kD * diffuse + specularIBL) * parameters.ambientOcclusion;

    float specular_contrib = (ambient + Lo).x * 0.5;
    
    float3 final = ambient + Lo;
    
    final = final / (final + vector_float3(1.0f));
   
    final = pow(final ,vector_float3(1.0f/ 2.2f));
    
    return float4(final, specular_contrib);

}

PBRParameter calculateParameters(v1f in,
                                 device const FrameData &frameData        [[ buffer(BufferIndexFrameData) ]],
                                 texture2d<float>   baseColorMap          [[ texture(TextureIndexBaseColor) ]],
                                 texture2d<float>   normalMap             [[ texture(TextureIndexNormal) ]],
                                 texture2d<float>   metallicMap           [[ texture(TextureIndexMetallic) ]],
                                 texture2d<float>   roughnessMap          [[ texture(TextureIndexRoughness) ]],
                                 texture2d<float>   ambientOcclusionMap   [[ texture(TextureIndexAmbientOcclusion) ]],
                                 texturecube<float> irradianceMap         [[ texture(TextureIndexIrradianceMap) ]],
                                 texturecube<float> prefilterMap          [[ texture(TextureIndexPreFilterMap)]],
                                 texture2d<float>   brdfMap               [[ texture(TextureIndexBDRF) ]],
                                 texture2d<float, access::sample> computedMap  [[ texture(TextureIndexWriteMap) ]])
{
    PBRParameter parameters;
   
    parameters.worldPos = in.worldPos;
    
    parameters.normal = computeNormalMap(in, normalMap);

    parameters.viewDir = normalize(frameData.cameraPos - parameters.worldPos);
    
    parameters.reflectedVector = reflect(-parameters.viewDir, parameters.normal);
    
    float3 baseColor = baseColorMap.sample(repeatSampler, in.texcoord).xyz;
    
    float3 computedColor = computedMap.sample(repeatSampler, in.texcoord).xyz;
    
    parameters.baseColor = pow(mix( baseColor , computedColor, in.colorMixBias), float3(2.0f));
    
    parameters.roughness = max(roughnessMap.sample(repeatSampler, in.texcoord.xy).x, 0.001f);
    parameters.roughness += in.roughnessBias;

    parameters.metalness = max(metallicMap.sample(repeatSampler, in.texcoord.xy).x, 0.1);
    parameters.metalness *= in.metallnessBias;
    
    parameters.ambientOcclusion = ambientOcclusionMap.sample(repeatSampler, in.texcoord.xy).x;

    float3 c = irradianceMap.sample(irradiatedSampler, parameters.normal).xyz ;
    parameters.irradiatedColor = clamp(c, 0.f, kMaxHDRValue);
    
    uint8_t mipLevel =  parameters.roughness * prefilterMap.get_num_mip_levels();
    parameters.prefilteredColor = prefilterMap.sample( preFilterSampler, parameters.reflectedVector , level(mipLevel)).xyz;

    parameters.nDotv = max(dot( parameters.normal, parameters.viewDir), 0.0);
    float2 brdfColor = brdfMap.sample(bdrfSampler, float2( parameters.nDotv, parameters.roughness)).xy;
    parameters.brdfColor = float4(brdfColor.x, brdfColor.y , 0.f, 1.f);

    parameters.sunEyeDirection = frameData.sun_eye_direction;
    parameters.sunPosition     = frameData.sunPosition.xyz;
    parameters.sunColor        = frameData.sun_color.xyz;
    parameters.sunPosition     = frameData.sunPosition.xyz;
    
    return parameters;
}

///--vertex function for calculation objects--------------------------------------------------------

vertex v1f vertexMain ( device const VertexData* vertexData       [[buffer(BufferIndexVertexData)]],
                        device const InstanceData* instanceData   [[buffer(BufferIndexInstanceData)]],
                        device const FrameData& frameData         [[buffer(BufferIndexFrameData)]],
                               uint vertexId                     [[vertex_id]],
                               uint instanceId                   [[instance_id]])
{
    v1f out;
    
    out.metallnessBias = frameData.metallnessBias;
    out.colorMixBias   = frameData.colorMixBias;
    out.roughnessBias  = frameData.roughnessBias;
    
    const device VertexData&vd = vertexData[ vertexId ];
    
    float4 pos = float4( vd.position, 1.0 );
    
    pos = instanceData[ instanceId ].instanceTransform * pos;
    
    out.position = frameData.perspectiveTransform * frameData.worldTransform *  pos;
    
    out.texcoord = vd.texCoord * frameData.textureScale;
    
    out.worldPos = pos.xyz;
    
    float3 normal = instanceData[ instanceId ].instanceNormalTransform * vd.normal;
    out.normal = normalize(frameData.worldNormalTransform * normal);

    float4 modelPos = float4( vd.position, 1.0);
    modelPos =  instanceData[ instanceId ].instanceTransform * modelPos;
    float3 shadow_coord =(frameData.shadow_xform_matrix * frameData.shadow_projections_matrix * frameData.shadow_view_matrix * modelPos).xyz;
    
    out.shadow_uv = shadow_coord.xy;
    out.shadow_depth = half(shadow_coord.z);
    
/// -- color
    half3 color = half3( instanceData[ instanceId ].instanceColor.rgb);
    out.color = color;

/// -- BDRF --


    return out;
};

///--fragment function for calculation textures light with reflecting effects-----------------------------
fragment GBufferData fragmentMain(v1f in [[stage_in]],
                             device const FrameData &frameData        [[ buffer(BufferIndexFrameData) ]],
                             texture2d<float>   baseColorMap          [[ texture(TextureIndexBaseColor) ]],
                             texture2d<float>   normalMap             [[ texture(TextureIndexNormal) ]],
                             texture2d<float>   metallicMap           [[ texture(TextureIndexMetallic) ]],
                             texture2d<float>   roughnessMap          [[ texture(TextureIndexRoughness) ]],
                             texture2d<float>   ambientOcclusionMap   [[ texture(TextureIndexAmbientOcclusion) ]],
                             texturecube<float> irradianceMap         [[ texture(TextureIndexIrradianceMap) ]],
                             texturecube<float> prefilterMap          [[ texture(TextureIndexPreFilterMap) ]],
                             texture2d<float>   brdfMap               [[ texture(TextureIndexBDRF) ]],
                             texture2d<float , access::sample> computeMap   [[ texture(TextureIndexWriteMap) ]],
                             depth2d<float>     shadowMap             [[ texture(TextureIndexShadowMap) ]])
{
    GBufferData gBuffer;
    
    PBRParameter parameters = calculateParameters(in ,
                                                  frameData,
                                                  baseColorMap,
                                                  normalMap,
                                                  metallicMap,
                                                  roughnessMap,
                                                  ambientOcclusionMap,
                                                  irradianceMap,
                                                  prefilterMap,
                                                  brdfMap,
                                                  computeMap);

    half3 eye_normal = normalize(half3(parameters.normal));
    
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
    
    half4 albedo_specular = half4(computeSpecular(parameters));
    gBuffer.albedo_specular = half4(albedo_specular.rgb, albedo_specular.w );
    gBuffer.normal_shadow = half4(eye_normal.xyz, shadow_sample);
    gBuffer.depth = in.worldPos.z;
    return gBuffer;
}

