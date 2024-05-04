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

inline double deg2rad(double deg)
{
    return deg * M_PI / 180.;
}

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
    _skyModel    = matrix4x4_scale_translation(scale, position);
};

- (nonnull instancetype)initWithPosition:(simd_float3)  position
                                rotation:(quaternion_float) rotation
                        sunLightPosition:(simd_float4) sunLightPosition
                               fovDegree:(float) fovDegree
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
        _aspect                  = aspect;
        _viewSize                = viewSize;
        _near                    = near;
        _far                     = far;
        _fovyRadians             = radians_from_degrees(fovDegree);
        _sunEyeDirection         = (simd_float4){0.f,0.f,0.f,0.f};
        _sunWorldPosition        = (simd_float4){0.f,0.f,0.f,0.f};
        _center                  = (simd_float3){0.f,0.f,0.f};
        _viewOffsetX             = (uint)0;
        _viewOffsetY             = (uint)0;
        _viewWidth               = (NSUInteger)0;
        _viewHeight              = (NSUInteger)0;
        _world                   = matrix_identity_float4x4;
        _projectionMatrix        = matrix_identity_float4x4;
        _shadowViewMatrix        = matrix_identity_float4x4;
        _shadowProjectionMatrix  = matrix_identity_float4x4;
        _direction               = (vector_float3){0.f};
        _skyModel                = matrix4x4_scale_translation((simd_float3){1.f,1.f,1.f},
                                                               (simd_float3){0.f,0.f,0.f});
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

- (const float) fovyRadians { return _fovyRadians;}

- (const float) far {return _far;}

- (const float) near {return _near;}

- (const float) viewSize {return _viewSize;}

- (const float) aspect {return _aspect;}

- (const simd_float4) sunLightPosition { return _sunLightPosition;}

- (const simd_float4) sunWorldPosition {
    
    _sunWorldPosition = simd_mul( [self skyModel] , [self sunLightPosition]);
    
    return _sunWorldPosition;
}

- (const simd_float4) sunEyeDirection {
    
    simd_float4 sunWorldDirection = - self.sunWorldPosition;
    _sunEyeDirection = simd_mul( [self world] , sunWorldDirection);
    
    return _sunEyeDirection;
}

- (const simd_float4x4) skyModel { return _skyModel;}

- (const NSUInteger) viewWidth {return _viewWidth;}

- (const NSUInteger) viewHeight {return _viewHeight;}

- (const uint) viewOffsetX {return _viewOffsetX;}

- (const uint) viewOffsetY {return _viewOffsetY;}


- (void) setAspect: (float)new_aspect{
    _dirty = YES;
    _aspect = new_aspect;
}
- (void) translate:(vector_float3)vec_dt {
    _dirty = YES;
    _translation += vec_dt;
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
    }
    return _world;
}

- (const simd_float3) updateDirection :(vector_float3) translation :(matrix_float4x4) rotationMatrix
{
    simd_float4 nDirection = simd_mul(rotationMatrix, (simd_float4){translation.x, translation.y, translation.z, 1});
    return nDirection.xyz;
}

-(const simd_float4x4) toShadowViewMatrix {
    
        simd_float4 directionalLightUpVector ={0.0, 1.0, 0.0 , 1.0};
        
        directionalLightUpVector = simd_mul( [self skyModel], directionalLightUpVector );
        directionalLightUpVector.xyz = simd_normalize(directionalLightUpVector.xyz);
        
        _shadowViewMatrix = matrix_look_at_left_hand (-[self sunWorldPosition].xyz,
                                                      (simd_float3){0,0,0},
                                                      directionalLightUpVector.xyz);
    
    return _shadowViewMatrix;
}

-(const matrix_float4x4) toProjectionMatrix{
    return _projectionMatrix;
}


-(const matrix_float4x4) createFrustumMatrix:(float)fov
                                            :(float)aspect
                                            :(float)near
                                            :(float)far
{
    float left   = -self.viewSize * aspect * 0.5;
    float right  =  self.viewSize * 0.5;
    float bottom = -self.viewSize * aspect;
    float top    =  self.viewSize;
    return matrix_ortho_left_hand( left,  right,  bottom,  top, near, far );
}

-(const matrix_float4x4) toShadowProjectionMatrix {
    
    const float aspect = 1.f;
    const float near   =  self.near;
    const float far    =  self.far ;
    
    _shadowProjectionMatrix = [self createFrustumMatrix: 45 : aspect : near :far];
    
    return _shadowProjectionMatrix;
}

- (const struct CameraData) updateCameraData
{
        _cameraData.viewMatrix = [self toViewMatrix];
        _cameraData.skyModel = self.skyModel;
        _cameraData.projectionMatrix = [self toProjectionMatrix];
        _cameraData.sunLightPosition= [self sunWorldPosition];
        _cameraData.sunEyeDirection = [self sunEyeDirection];
        _cameraData.cameraPosition = self.translation;
        _cameraData.cameraDirection = self.direction;
        _cameraData.fovyRadians = self.fovyRadians;
        _cameraData.aspect = self.aspect;
        _cameraData.far = self.far;
        _cameraData.near = self.near;
        _cameraData.center = self.center;
        _cameraData.viewSize = self.viewSize;
    return _cameraData;
}

- (const struct CameraData) updateShadowCameraData
{
        _shadowCameraData.viewMatrix = [self toShadowViewMatrix];
        _shadowCameraData.projectionMatrix = [self toShadowProjectionMatrix];
    
    return _shadowCameraData;
}

- (const struct FrustumPoints) calculatePlane:(simd_float4x4)viewMatrix
                                             :(float)distance {
    
    float aspect = self.aspect;
    float halfHeight = self.viewSize * 0.5;
    float halfWidth = halfHeight * aspect;
   
    return [self calculatePlanePoints :viewMatrix
                                      :halfWidth
                                      :halfHeight
                                      :distance
                                      :self.sunWorldPosition.xyz];
    
}

- (const struct FrustumPoints) calculatePlanePoints :(simd_float4x4) matrix
                                                    :(float)halfWidth
                                                    :(float)halfHeight
                                                    :(float)distance
                                                    :(simd_float3)position{
    
    simd_float3 forwardVector = {matrix.columns[0].z, matrix.columns[1].z, matrix.columns[2].z};
    simd_float3 rightVector = {matrix.columns[0].x, matrix.columns[1].x, matrix.columns[2].x};
    simd_float3 upVector = simd_cross(forwardVector, rightVector);
    simd_float3 centerPoint = position + forwardVector * distance;
    simd_float3 moveRightBy = rightVector * halfWidth;
    simd_float3 moveDownBy = upVector * halfHeight;
    
    simd_float3 upperLeft = centerPoint - moveRightBy + moveDownBy;
    simd_float3 upperRight = centerPoint + moveRightBy + moveDownBy;
    simd_float3 lowerRight = centerPoint + moveRightBy - moveDownBy;
    simd_float3 lowerLeft = centerPoint - moveRightBy - moveDownBy;
    
    struct FrustumPoints points = {(simd_float4x4)matrix,
            (simd_float3) upperLeft,
            (simd_float3) upperRight,
            (simd_float3) lowerRight,
            (simd_float3) lowerLeft
    };
    
    return points;
    
}
/*
-(const matrix_float4x4) toShadowProjectionMatrix {
    
    // create shadow camera using bounding sphere
    float viewSize = self.viewSize;
    
    const float aspect = self.aspect;
    
    float near   = - 1.f;
    
    float far    =  self.far ;
   
    float left   = -viewSize * aspect * 0.5;
    float right  =  viewSize * 0.5;
    float bottom = -viewSize * aspect;
    float top    =  viewSize;
    
    
    float dx = (float){ right - left };
    float dy = (float){ top - bottom };
    float cx = (float){ right + left };
    float cy = (float){ top + bottom };
    
    float Left      = (float) {cx - dx};
    float Right     = (float) {cx + dx};
    float Bottom    = (float) {cy - dy};
    float Top       = (float) {cy + dy};
    
   // float scaleW = (float)( Right - Left ) / self.viewWidth;
   // float scaleH = (float)( Top - Bottom ) / self.viewHeight;
    
     _shadowProjectionMatrix = matrix_ortho_left_hand( Left , Right , Bottom , Top , near , far );
    
    return _shadowProjectionMatrix;
}

- (const matrix_float4x4)setFrustum:(float)fov :(float)aspect :(float)front :(float)back {}
    */

@end
