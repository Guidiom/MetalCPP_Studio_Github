//
//  AAPLShaderCommon.h
//  MetalCPP
//
//  Created by Guido Schneider on 08.10.23.

#ifndef AAPLShaderCommon_h
#define AAPLShaderCommon_h

#include "AAPLShaderTypes.h"
/// Raster order group definitions
#define LightingROG  0
#define GBufferROG   1

// G-buffer outputs using Raster Order Groups
struct GBufferData
{
    half4 lighting        [[ color(RenderTargetLighting), raster_order_group(LightingROG) ]];
    half4 albedo_specular [[ color(RenderTargetAlbedo),   raster_order_group(GBufferROG) ]];
    half4 normal_shadow   [[ color(RenderTargetNormal),   raster_order_group(GBufferROG) ]];
    float depth           [[ color(RenderTargetDepth),    raster_order_group(GBufferROG) ]];
};

// Final buffer outputs using Raster Order Groups
struct AccumLightBuffer
{
    half4 lighting [[ color(RenderTargetLighting), raster_order_group(LightingROG) ]];
};

#endif /* AAPLShaderCommon_h */
