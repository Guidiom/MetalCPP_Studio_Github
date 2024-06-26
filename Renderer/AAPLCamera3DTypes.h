//
//  AAPLCamera3DTypes.h
//  MetalCPP
//
//  Created by Guido Schneider on 12.01.24.
//

#ifndef AAPLCamera3DTypes_h
#define AAPLCamera3DTypes_h

typedef struct CameraData{
    float                fovyRadians;
    float                     aspect;
    float                   viewSize;
    float                       near;
    float                        far;
    simd_float4     sunLightPosition;
    simd_float4      sunEyeDirection;
    simd_float3               center;
    simd_float3      cameraDirection;
    simd_float3       cameraPosition;
    simd_float4x4           skyModel;
    simd_float4x4         viewMatrix;
    matrix_float4x4 projectionMatrix;
}CameraData;

struct FrustumPoints {
    simd_float4x4 viewMatrix;
    simd_float3 upperLeft;
    simd_float3 upperRight;
    simd_float3 lowerRight;
    simd_float3 lowerLeft;
};

#endif /* AAPLCamera3DTypes_h */
