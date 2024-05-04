///
///  AAPLRenderAdapter.cpp
///  MetalCPP
///
///  Created by Guido Schneider on 11.08.22.

#if __has_feature(objc_arc)
#error This file must be compiled with -fno-objc-arc
#endif

#include "AAPLRenderAdapter.h"
#include "Renderer.h"

@interface AAPLRenderAdapter()
{
    Renderer* _Nonnull      _pRenderer;
    MTL::Device* _Nonnull     _pDevice;
    AAPLCamera3D* _Nonnull      _camera;
    MTLPrimitiveType    _primitiveType;
}
@end

@implementation AAPLRenderAdapter

- (instancetype)initWithMTKView:(MTKView*)pMTKView
{
    self = [super init];
    
    if(self)
    {
        _pDevice   = ((__bridge MTL::Device *) pMTKView.device);
        
        _pRenderer = new Renderer(* ((__bridge MTK::View *) pMTKView));
        
        float _aspect = (float)pMTKView.bounds.size.width /  float(pMTKView.bounds.size.height != 0 ? pMTKView.bounds.size.height : 1);
        
        _camera  = [[AAPLCamera3D alloc] initWithPosition: simd_float3 { 0.0f, 4.0f, 3.0f}
                                                 rotation: quaternion_identity()
                                         sunLightPosition: simd_float4 { -0.25f , -2.0f , -3.0f , 0.f }
                                                fovDegree: float {45.f}
                                                   aspect: (float)_aspect
                                                 viewSize: float {10.f}
                                                     near: float {0.1f}
                                                      far: float {200.f}];
        
        
        
        self.camera = _camera;
        
    }
    return self;
}
 
- (void)dealloc
{    
    [super dealloc];
}

- (nonnull void*)device {
    return (void*)_pDevice;
}

- (void)drawInMTKView:(nonnull MTKView*) pMTKView{
    
    [self updateCameras];
    
    //NSLog( @"cameraViewPortWidth: %lu", (unsigned long) self.camera.viewWidth );
    //NSLog( @"cameraViewPortHeight: %lu", (unsigned long) self.camera.viewHeight );
    //NSLog( @"cameraViewPortoffsetX: %lu", (unsigned long) self.camera.viewOffsetX );
    //NSLog( @"cameraViewPortoffsetY: %lu", (unsigned long) self.camera.viewOffsetY );
    
    _pRenderer->drawInView((__bridge MTK::View*) pMTKView,
                           (__bridge MTL::Drawable*) pMTKView.currentDrawable,
                           (__bridge MTL::Texture*) pMTKView.depthStencilTexture);
}

- (void)drawableSizeWillChange:(CGSize)size {
    
    const MTL::Size nSize = MTL::Size::Make(size.width, size.height, 1);
    float aspect = (float)nSize.width / float(nSize.height  != 0 ? nSize.height : 1);
    [self.camera setViewWidth: NS::UInteger(nSize.width)];
    [self.camera setViewHeight: NS::UInteger(nSize.height)];
    [self.camera setAspect : aspect];
    [self updateCameras];
    
    _pRenderer->drawableSizeWillChange(nSize);
    
}

- ( const simd::float2 ) cursorPosition {
    _pRenderer->setCursorPosition( simd_float2 {static_cast<float>(_cursorPosition.x),static_cast<float>(_cursorPosition.y)});
    return _cursorPosition;
}

-( const NSUInteger ) mouseButtonMask {
    _pRenderer->setMouseButtonMask(static_cast<NS::UInteger>(_mouseButtonMask));
    return _mouseButtonMask;
}

- (const MTLPixelFormat) colorPixelFormat {
    _pRenderer->setColorTargetPixelFormat((__bridge MTL::PixelFormat)_colorPixelFormat);
    return _colorPixelFormat;
}

- (const MTLPixelFormat) depthStencilPixelFormat {
    _pRenderer->setDepthStencilTargetPixelFormat((__bridge MTL::PixelFormat) _depthStencilPilxelFormat);
    return _depthStencilPilxelFormat;
}

- (const MTLPrimitiveType) primitiveType {
    return _primitiveType;
}

- (void) setPrimitiveType:(MTLPrimitiveType) primitiveType {
    _primitiveType = primitiveType;
    _pRenderer->buildBuffers();
    _pRenderer->setPrimitiveType((__bridge MTL::PrimitiveType) _primitiveType);
}

- (void) setInstanceRows:(int) sender {
    _pRenderer->setInstances(size_t(sender), size_t(sender), size_t(sender));
    _pRenderer->buildBuffers();
}

- (void) setInstanceSize:(float) sender {
    _pRenderer->setInstancesSize( static_cast<float>(sender) );
    _pRenderer->buildBuffers();
    _pRenderer->buildParticleBuffer();
}

- (void) setGroupScale:(float) sender {
    _pRenderer->setGroupScale( static_cast<float>(sender) );
    _pRenderer->buildBuffers();
}

- (void) setTextureScale:(float) scale {
    _pRenderer->setTextureScale(static_cast<float>(scale));
}

- (void) updateCameras {
    _pRenderer->setCameraData(static_cast<CameraData>([self.camera updateCameraData]));
    _pRenderer->setShadowCameraData(static_cast<CameraData>([self.camera updateShadowCameraData]));
}

- (void)setBaseColorMixValue:(float)sender {
    _pRenderer->setBaseColorMixValue( static_cast<float>(sender) );
}

- (void)setMetallTextureValue:(float)sender {
    _pRenderer->setMetallTextureValue( static_cast<float>(sender) );
}

- (void)setRoughnessTextureValue:(float)sender {
    _pRenderer->setRoughnessTextureValue( static_cast<float>(sender) );
}

- (void)setStepPerFrameValue:(float)sender {
    _pRenderer->setStepPerFrameValue( static_cast<float>(sender) );
}

- (void)setFamilyValue:(int)sender {
    _pRenderer->setNumFamilies( static_cast<int>(sender));
    _pRenderer->buildParticleBuffer();
}

- (void)setTurnSpeedValue:(float)sender {
    _pRenderer->setTurnSpeedValue( static_cast<float>(sender) );
}

- (void)setEvaporationValue:(float)sender {
    _pRenderer->setEvaporationValue( static_cast<float>(sender) );
}

- (void) setSenseAngleValue:(float) sender {
    _pRenderer->setSenseAngleValue( static_cast<float>(sender) );
}

- (void) setSenseOffsetValue:(float) sender {
    _pRenderer->setSenseOffsetValue( static_cast<float>(sender) );
}

- (void) setTrailWeightValue:(float) sender {
    _pRenderer->setTrailWeightValue( static_cast<float>(sender) );
}

@end
