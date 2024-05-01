///
/// AAPLShaderTypes.h
/// MetalCCP
/// Created by Guido Schneider on 29.07.22.
///
#ifndef AAPLSHADERTYPES_H
#define AAPLSHADERTYPES_H

#ifndef __METAL_VERSION__
/// 96-bit / 12 byte 3 component float vector type
typedef struct __attribute__ ((packed)) packed_float3 {
    float x;
    float y;
    float z;
}packed_float3;
#endif

#include <simd/simd.h>
#include "AAPLShaderTypes.h"

#define PI 3.1415926535897932384626433832795



///enums
enum BufferIndex : int32_t
{
    BufferIndexVertexData       = 0,
    BufferIndexInstanceData     = 1,
    BufferIndexFrameData        = 2,
    BufferIndexAnimationData    = 3,
    BufferIndexParticleData     = 4,
    BufferIndexUniformData      = 5,
    BufferIndexTimeData         = 6,
    BufferIndexInterActionData  = 7,
    BufferIndexShadowVertex     = 8,
    BufferIndexQuadVertexData   = 9,
    BufferIndexGroundVertexData = 10,
    BufferIndexLightData        = 11,
    BufferIndexLightsPosition   = 12,
    BufferIndexPointVertexData  = 13,
    BufferIndexMeshPositions    = 14,
    BufferIndexMeshGenerics     = 15,
};

typedef enum VertexAttributes
{
    VertexAttributePosition   = 0,
    VertexAttributeTexcoord   = 1,
    VertexAttributeNormal     = 2,
    VertexAttributeTangent    = 3,
    VertexAttributeBitangent  = 4,
} VertexAttributes;

/// Texture index values shared between shader and C code to ensure Metal shader texture indices
/// match indices of Metal API texture set calls

enum TextureIndex : int32_t
{
    TextureIndexBaseColor        =  0,
    TextureIndexNormal           =  1,
    TextureIndexMetallic         =  2,
    TextureIndexRoughness        =  3,
    TextureIndexAmbientOcclusion =  4,
    TextureIndexIrradianceMap    =  5,
    TextureIndexPreFilterMap     =  6,
    TextureIndexBDRF             =  7,
    TextureIndexSkyMap           =  8,
    TextureIndexReadMap          =  9,
    TextureIndexWriteMap         = 10,
    TextureIndexComputedMap      = 11,
    TextureIndexShadowMap        = 12,
    TextureIndexPointMap         = 13,
    TextureIndexAlpha            = 14,
};

enum RenderTargetIndex : int32_t
{
    RenderTargetLighting  = 0,
    RenderTargetAlbedo    = 1,
    RenderTargetNormal    = 2,
    RenderTargetDepth     = 3
};

struct VertexData
{
    simd::float3 position;
    simd::float2 texCoord;
    simd::float3 normal; 
};

struct InstanceData
{
    simd::float4x4 instanceTransform;
    simd::float3x3 instanceNormalTransform;
    simd::float4   instanceColor;
};

struct PointLightData
{
    vector_float3 pointLightColor;
    float light_radius;
    float light_speed;
};

struct FrameData
{
    ///Per Frame Data
    vector_float3  cameraPos;
    vector_float3  cameraDir;
    simd::float4x4 viewMatrix;
    simd::float4x4 worldTransform;
    simd::float3x3 worldNormalTransform;
    simd::float4x4 perspectiveTransform;
    simd::float4x4 projection_matrix_inverse;
    simd::float4x4 sky_modelview_matrix;
    simd::float4x4 skyModelMatrix;
    simd::float4x4 planeModelViewMatrix;
    simd::float3x3 normalPlaneModelViewMatrix;
    simd::float4x4 scaleMatrix;
    
    uint framebuffer_width;
    uint framebuffer_height;

    float point_size;
    float point_specular_intensity;
    
    // sun
    simd::float4 sun_color;
    simd::float4 sun_eye_direction;
    simd::float4 sunPosition;
    float shininess_factor;
    float sun_specular_intensity;
    
    /// Shadow paras
    simd::float4x4 shadow_view_matrix;
    simd::float4x4 shadow_projections_matrix;
    simd::float4x4 shadow_xform_matrix;
    
    /// Per Texture Transform
    float textureScale;
    float colorMixBias;
    float metallnessBias;
    float roughnessBias;
    float mipLevel;
};

struct Particle
{
    uint active;
    simd::float2 position;
    float dir;
    simd::int4 families;
};

struct Uniforms 
{
    uint particleCount;
    float sensorOffset;
    float sensorAngle;
    float moveSpeed;
    uint sensorSize;
    float turnSpeed;
    float evaporation;
    float trailWeight;
    simd::uint2 Dimensions;
    uint family;
};

struct GroundVertex
{
    simd::float4 position;
    simd::float3 normal;
};

struct SimpleVertex
{
    simd::float2 position;
};

struct ShadowVertex
{
    packed_float3 position;
};

#endif // AAPLSHADERTYPES_H

