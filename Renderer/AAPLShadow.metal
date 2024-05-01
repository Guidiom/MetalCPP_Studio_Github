//
//  AAPLShadow.metal
//  MetalCPP
//  Created by Guido Schneider on 06.09.23.
//
//Metal shaders used to render shadow maps
#include <metal_stdlib>
using namespace metal;

#include "AAPLShaderTypes.h"

struct ShadowOutput
{
    float4 position [[position]];
    float2 texcoord;
};

vertex ShadowOutput shadow_vertex( const device VertexData *vertexData    [[buffer(BufferIndexVertexData) ]],
                                   const device InstanceData *instanceData[[buffer(BufferIndexInstanceData)]],
                                   constant  FrameData & frameData        [[buffer(BufferIndexFrameData)]],
                                   uint vertexID                          [[vertex_id]],
                                   uint instanceId                        [[instance_id]])
{
    ShadowOutput out;
    
    device const VertexData & vd = vertexData[ vertexID ];
    float4 pos = float4( vd.position, 1.0 );
    pos = instanceData[instanceId].instanceTransform * pos;
    out.position = frameData.shadow_projections_matrix * frameData.shadow_view_matrix * pos;
    
    out.texcoord = vd.texCoord;
    
    return out;
}
