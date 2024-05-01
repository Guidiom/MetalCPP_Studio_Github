/*
 See LICENSE folder for this sample’s licensing information.
 
 Abstract:
 Metal shaders used to render skybox
 */
#include <metal_stdlib>

using namespace metal;

     // Include header shared between this Metal shader code and C code executing Metal API commands
#include "AAPLShaderTypes.h"

     // Per-vertex inputs fed by vertex buffer laid out with MTLVertexDescriptor in Metal API
struct MeshVertex
{
     float4 position [[attribute(VertexAttributePosition)]];
     float3 normal   [[attribute(VertexAttributeNormal)]];
};

struct MeshInOut
{
     float4 position [[position]];
     float3 texcoord;
};

vertex MeshInOut mesh_vertex(MeshVertex        in        [[ stage_in ]],
                             constant CameraData &cameraData [[ buffer(BufferIndexCameraData) ]])
{
     MeshInOut out;
     
          // Add vertex pos to fairy position and project to clip-space
     out.position = cameraData.perspectiveTransform * cameraData.worldTransform * in.position;
     
          // Pass position through as texcoord
     out.texcoord = in.normal;
     
     return out;
}

fragment half4 mesh_fragment(MeshInOut        in             [[ stage_in ]],
                             texturecube<float> mesh_texture [[ texture(TextureIndexBaseColor) ]])
{
     constexpr sampler linearSampler(mip_filter::linear, mag_filter::linear, min_filter::linear);
     
     float4 color = mesh_texture.sample(linearSampler, in.texcoord);
     
     return half4(color);
}

