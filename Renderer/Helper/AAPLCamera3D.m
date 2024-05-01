///  multi plattform application ---
///---------------------------------
///  author : Guido Schneider
///  copyrights by Guido Schneider
///  2022 Berlin
///---------------------------------
///  ---  MetelCPP  ---
///---------------------------------



#include <simd/simd.h>

#include "AAPLCamera3D.h"

// First generate the full matrix basis, then write out an inverse(!) transform matrix

@implementation AAPLCamera3D : NSObject {};

- (const simd_float4x4)sInvMatrixLookat:(simd_float3)inEye :(simd_float3)inTo :(simd_float3)inUp
{
    simd_float4x4 iVL = makeIdentity();
    
    if (_dirty == YES){
        _dirty = NO;
        simd_float3 z = simd_normalize(inTo - inEye);
        simd_float3 x = simd_normalize(simd_cross(inUp, z));
        simd_float3 y = simd_cross(z, x);
        simd_float3 t = (simd_float3) { -simd_dot(x, inEye), -simd_dot(y, inEye), -simd_dot(z, inEye)};
        
        iVL = (simd_float4x4) { (simd_float4) { x.x, y.x, z.x, 0 },
            (simd_float4) { x.y, y.y, z.y, 0 },
            (simd_float4) { x.z, y.z, z.z, 0 },
            (simd_float4) { t.x, t.y, t.z, 1 } };
    }
    return iVL;
}

- (void) setSkyModel: (simd_float3) scale : (simd_float3) position {
    _skyModel    = matrix4x4_scale_translation((vector_float3){1,1,1}, (vector_float3){0,0,0});
};

- (nonnull instancetype)initWithPosition:(simd_float3)  position
                                rotation:(quaternion_float) rotation
                        sunLightPosition:(simd_float4) sunLightPosition
                                  aspect:(float) aspect
                                viewSize:(float) viewSize
                                    near:(float) near
                                     far:(float) far
{
    self = [super init];
    
    if(self)
    {
        _translation             = position;
        _rotation                = rotation;
        _sunLightPosition        = sunLightPosition;
        _sunEyeDirection         = (simd_float4){0.f};
        _aspect                  = aspect;
        _viewSize                = viewSize;
        _near                    = near;
        _far                     = far;
        _center                  = (simd_float3){0.f};
        _viewOffsetX             = (uint)0;
        _viewOffsetY             = (uint)0;
        _viewWidth               = (NSUInteger)0;
        _viewHeight              = (NSUInteger)0;
        _world                   = matrix_identity_float4x4;
        _shadowViewMatrix        = matrix_identity_float4x4;
        _shadowProjectionMatrix  = matrix_identity_float4x4;
        _direction               = (vector_float3){0.f};
        
        [self setSkyModel:(simd_float3){1.f,1.f,1.f} :(simd_float3){0.f,0.f,0.f}];
        
        _cameraData.aspect = _aspect;
        _cameraData.viewSize = _viewSize;
        _cameraData.near = _near;
        _cameraData.far = _far;
        _cameraData.center = _center;
        _cameraData.cameraPosition = _translation;
        _cameraData.cameraDirection = _direction;
        _cameraData.sunLightPosition = _sunLightPosition;
        _cameraData.skyModel = _skyModel;
        _cameraData.viewMatrix = _world;
        _dirty                 = NO;
    }
    return self;
}


// Axis in vector_float3
- (const vector_float3)       LocalForward { return (vector_float3) { 0.f,  0.f, -1.f };}
- (const vector_float3)            LocalUp { return (vector_float3) { 0.f,  1.f,  0.f };}
- (const vector_float3)         LocalRight { return (vector_float3) { 1.f,  0.f,  0.f };}

// Queries
- (const vector_float3)       forward { return quaternion_rotate_vector( _rotation, self.LocalForward );}
- (const vector_float3)            up { return quaternion_rotate_vector( _rotation, self.LocalUp );}
- (const vector_float3)         right { return quaternion_rotate_vector( _rotation, self.LocalRight);}

- (const simd_float3) center { return _center;}

- (const float) far {return _far;}

- (const float) near {return _near;}

- (const float) viewSize {return _viewSize;}

- (const float) aspect {return _aspect;}

- (const simd_float4) sunLightPosition { return _sunLightPosition;}

- (const simd_float4) sunEyeDirection { return _sunEyeDirection;}

- (const simd_float4x4) skyModel { return _skyModel;}

- (const NSUInteger) viewWidth {return _viewWidth;}

- (const NSUInteger) viewHeight {return _viewHeight;}

- (const uint) viewOffsetX {return _viewOffsetX;}

- (const uint) viewOffsetY {return _viewOffsetY;}

- (void) setViewPortSize:(const NSUInteger)width :(const NSUInteger)height
                        :(const uint) offsetX :(const uint) offsetY {
    _dirty = YES;
    if(_viewWidth != width) { _viewWidth = width;}
    if(_viewHeight != height){ _viewHeight = height;}
    if(_viewOffsetX != offsetX){_viewOffsetX = offsetX;}
    if(_viewOffsetY != offsetY){_viewOffsetY = offsetY;}
}

- (void) translate:(vector_float3)vec_dt {
    _dirty = YES;
    _translation += vec_dt;;
}

- (void) translate:(float)vec_dx :(float)vec_dy :(float)vec_dz {
    _dirty = YES;
    [self translate: (vector_float3) { vec_dx , vec_dy, vec_dz }];
}

- (void) rotate:(quaternion_float) qua_dr {
    _dirty = YES;
    _rotation = quaternion_multiply (qua_dr, _rotation);
}

- (void) rotate:(float)angle :(vector_float3) axis {
    _dirty = YES;
    [self rotate: quaternion_from_axis_angle( axis, angle )];
}

- (void) rotate:(float)angle :(float)angx :(float)angy :(float)angz{
    _dirty = YES;
    [self rotate: quaternion_from_axis_angle ((vector_float3){angx, angy, angz}, angle)];
}

- (void) direction:(vector_float3)vec_dir {
    _dirty = YES;
    _direction = add(_direction, vec_dir);
}

- (void) direction:(float)vec_dir_x :(float)vec_dir_y :(float)vec_dir_z {
    _dirty = YES;
    [self direction:(vector_float3){ vec_dir_x , vec_dir_y, vec_dir_z}];
}

- (void)setTranslation:(const vector_float3)newTranslation{
    _dirty = YES;
    _translation = newTranslation;
}

- (void)setTranslation:(float)vecx :(float)vecy :(float)vecz{
    _dirty = YES;
    [self setTranslation:(vector_float3){vecx, vecy, vecz}];
}

- (void) setRotation:(const quaternion_float) newRotation{
    _dirty = YES;
    _rotation = newRotation;
}

- (void) setRotation:(float)newRot_x :(float)newRot_y :(float)newRot_z :(float)newRot_w{
    _dirty = YES;
    [self setRotation: (quaternion_float) { newRot_x , newRot_y , newRot_z , newRot_w }];
}

- (void) setDirection:(vector_float3) newDir{
    _dirty = YES;
    _direction = newDir;
}

- (void) setDirection:(float)newDir_x :(float)newDir_y :(float)newDir_z {
    _dirty = YES;
    [self setDirection: (vector_float3){newDir_x, newDir_y, newDir_z}];;
}

/// Accessors

- (const simd_float4x4) toViewMatrix {
    if(_dirty == YES){
        _dirty = NO;
        simd_float4x4 translationMatrix  = matrix4x4_translation(-self.translation);
        simd_float4x4 rotationMatrix     = matrix4x4_from_quaternion(quaternion_conjugate(self.rotation));
        simd_float4x4 scaleMatrix        = matrix4x4_scale(1.f, 1.f, 1.f);
        _world  = matrix_multiply(matrix_multiply( rotationMatrix, translationMatrix), scaleMatrix);
        _direction = [self updateDirection: self.translation : rotationMatrix ];
        _sunLightPosition = simd_mul( [self skyModel]  , self.sunLightPosition);
    }
    return _world;
}

- (const simd_float3) updateDirection :(vector_float3) translation :(matrix_float4x4) rotationMatrix
{
    return  simd_mul( matrix3x3_upper_left(rotationMatrix), translation);
}

-(const simd_float4x4) toShadowViewMatrix {
    
    vector_float4 sunWorldPosition = self.sunLightPosition;
    sunWorldPosition = simd_mul( [self skyModel]  , sunWorldPosition);
    
    vector_float4 sunWorldDirection = -sunWorldPosition;
    
    _sunEyeDirection = matrix_multiply( [self world] , sunWorldDirection);
    
    simd_float4 directionalLightUpVector ={0.0, 1.0, 0.0, 0.0};
    
    directionalLightUpVector = matrix_multiply( [self skyModel], directionalLightUpVector );
    directionalLightUpVector.xyz = simd_normalize(directionalLightUpVector.xyz);
    
    _shadowViewMatrix = matrix_look_at_left_hand (sunWorldDirection.xyz,
                                                  (simd_float3){0,0,0},
                                                  directionalLightUpVector.xyz);
    return _shadowViewMatrix;
}


-(const matrix_float4x4) toShadowProjectionMatrix {
   
    // const float aspect =  self.aspect;
    float near   =  -1.f;
    float far    =  150.f;
    float left   = -self.viewSize;
    float right  =  self.viewSize;
    float bottom = -self.viewSize;
    float top    =  self.viewSize;
    /*
    float dx = (float){ right - left };
    float dy = (float){ top - bottom };
    float cx = (float){ right + left };
    float cy = (float){ top + bottom };
    
    float Left      = (float) {cx - dx};
    float Right     = (float) {cx + dx};
    float Bottom    = (float) {cy - dy};
    float Top       = (float) {cy + dy};
    
    //float scaleW = (float)( Right - Left ) / self.viewWidth;
    //float scaleH = (float)( Top - Bottom ) / self.viewHeight;
    */
    _shadowProjectionMatrix = matrix_ortho_left_hand( left , right , bottom , top , near , far );
    
    return _shadowProjectionMatrix;
}

- (const struct CameraData) updateCameraData
{
        _cameraData.viewMatrix = [self toViewMatrix];
        _cameraData.skyModel = self.skyModel;
        _cameraData.sunLightPosition= self.sunLightPosition;
        _cameraData.sunEyeDirection = self.sunEyeDirection;
        _cameraData.cameraPosition = self.translation;
        _cameraData.cameraDirection = self.direction;
        _cameraData.aspect = self.aspect;
        _cameraData.far = self.far;
        _cameraData.near = self.near;
        _cameraData.viewSize = self.viewSize;
    return _cameraData;
}

- (const struct CameraData) updateShadowCameraData
{
        _shadowCameraData.viewMatrix = [self toShadowViewMatrix];
        _shadowCameraData.projectionMatrix = [self toShadowProjectionMatrix];
    
    return _shadowCameraData;
}

@end
