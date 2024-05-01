///  multi plattform application ---
///---------------------------------
///  author : Guido Schneider
///  copyrights by Guido Schneider
///  2022 Berlin
///---------------------------------
///  MetalCCP
///---------------------------------

#import <Foundation/Foundation.h>
#import "CoreGraphics/CoreGraphics.h"
#import <simd/simd.h>
#include "AAPLMathUtilities.h"
#include "AAPLCamera3DTypes.h"


@interface AAPLCamera3D : NSObject {
    
    BOOL                              _dirty;
    vector_float3               _translation;
    quaternion_float               _rotation;
    simd_float3                   _direction;
    simd_float4            _sunLightPosition;
    simd_float4             _sunEyeDirection;
    simd_float4x4                     _world;
    simd_float4x4                  _skyModel;
    simd_float4x4          _shadowViewMatrix;
    matrix_float4x4    _shadowProjectionMatrix;
    struct CameraData            _cameraData;
    struct CameraData      _shadowCameraData;
    float                            _aspect;
    float                               _far;
    float                              _near;
    simd_float3                      _center;
    float                          _viewSize;
    NSUInteger                    _viewWidth;
    NSUInteger                   _viewHeight;
    uint                        _viewOffsetX;
    uint                        _viewOffsetY;
}

@property (nonatomic, assign, readwrite) const BOOL dirty;

@property (NS_NONATOMIC_IOSONLY, readonly) const vector_float3 LocalForward;

@property (NS_NONATOMIC_IOSONLY, readonly) const vector_float3 LocalUp;

@property (NS_NONATOMIC_IOSONLY, readonly) const vector_float3 LocalRight;

///--Queries
@property (nonatomic, assign, readwrite) const vector_float3 forward;

@property (nonatomic, assign, readwrite) const vector_float3 up;

@property (nonatomic, assign, readwrite) const vector_float3 right;

@property (nonatomic, assign, readwrite) const vector_float3 translation;

@property (nonatomic, assign, readwrite) const simd_float3 direction;

@property (nonatomic, assign, readwrite) const quaternion_float rotation;

@property (nonatomic, assign, readwrite) const simd_float4x4 world;

@property (nonatomic, assign, readwrite) const simd_float4x4 skyModel;

@property (nonatomic, assign, readwrite) const simd_float4x4 shadowViewMatrix;

@property (nonatomic, assign, readwrite) const matrix_float4x4 shadowProjectionMatrix;

@property (nonatomic, assign, readwrite) const float aspect;

@property (nonatomic, assign, readwrite) const float viewSize;

@property (nonatomic, assign, readwrite) const float far;

@property (nonatomic, assign, readwrite) const float near;

@property (nonatomic, assign, readwrite) const simd_float3 center;

@property (nonatomic, assign, readwrite) const simd_float4 sunEyeDirection;

@property (nonatomic, assign, readwrite) const simd_float4 sunLightPosition;

@property (nonatomic, assign, readwrite) const NSUInteger viewWidth;

@property (nonatomic, assign, readwrite) const NSUInteger viewHeight;

@property (nonatomic, assign, readwrite) const uint viewOffsetX;

@property (nonatomic, assign, readwrite) const uint viewOffsetY;

@property (nonatomic, assign, readwrite) struct CameraData shadowCameraData;

@property (nonatomic, assign, readwrite) struct CameraData cameraData;

-(nonnull instancetype)initWithPosition:(simd_float3) position
                               rotation:(quaternion_float) rotation
                       sunLightPosition:(simd_float4) sunLightPosition
                                 aspect:(float) aspect
                               viewSize:(float) viewSize
                                   near:(float) near
                                    far:(float) far;

- (void) setViewPortSize:(const NSUInteger) width :(const NSUInteger) height 
                        :(const uint) offsetX :(const uint) offsetY;

/// Transform By (Add/Scale)
- (void) translate:(vector_float3) vec_dt;
- (void) translate:(float)vec_dx :(float)vec_dy :(float)vec_dz;

/// Transform To (Setters)
- (void) setTranslation:(const vector_float3 ) vec_t;
- (void) setTranslation:(float)vecx :(float)vecy :(float)vecz;

/// Rotate by (Add angles)
- (void) rotate:(quaternion_float) qua_dr;
- (void) rotate:(float)angle :(vector_float3) axis;
- (void) rotate:(float)angle :(float)angx :(float)angy :(float)angz;

/// Rotate To (Setters)
- (void) setRotation:(const quaternion_float) newRotation;
- (void) setRotation:(float)newRot_x :(float)newRot_y :(float)newRot_z :(float)newRot_w;

- (void) direction:(vector_float3) vec_dir;
- (void) direction:(float)vec_dir_x :(float)vec_dir_y :(float)vec_dir_z;

- (void) setDirection:(vector_float3) newDir;
- (void) setDirection:(float)newDir_x :(float)newDir_y :(float)newDir_z;

- (void) setSkyModel:(simd_float3) scale :(simd_float3) position;

/// Accessors
- (const simd_float4x4) sInvMatrixLookat:(simd_float3)inEye
                                        :(simd_float3)inTo
                                        :(simd_float3)inUp;

- (const vector_float3)                 translation;
- (const vector_float3)                   direction;
- (const quaternion_float)                 rotation;
- (const simd_float4)              sunLightPosition;
- (const simd_float4)               sunEyeDirection;
- (const float)                                 far;
- (const float)                                near;
- (const float)                            viewSize;
- (const float)                              aspect;
- (const NSUInteger)                     viewHeight;
- (const NSUInteger)                      viewWidth;
- (const uint)                          viewOffsetX;
- (const uint)                          viewOffsetY;
- (const simd_float3)                        center;
- (const simd_float4x4)                     skyModel;
- (const simd_float4x4)                 toViewMatrix;
- (const simd_float3)         updateDirection:(vector_float3)translation
                                             :(matrix_float4x4) rotationMatrix;

- (const simd_float4x4)           toShadowViewMatrix;
- (const matrix_float4x4)     toShadowProjectionMatrix;
- (const struct CameraData)         updateCameraData;
- (const struct CameraData)   updateShadowCameraData;

@end

