///
///  AAPLRenderAdapter.hpp
///  MetalCPP
///
///  Created by Guido Schneider on 11.08.22.
#pragma once

//#import "TargetConditionals.h"
#import <Foundation/NSObject.h>
#import <CoreGraphics/CGGeometry.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import "AAPLCamera3D.h"
#import <simd/simd.h>

@class MTKView;

@interface AAPLRenderAdapter : NSObject
    
@property (nonatomic, assign, readwrite) AAPLCamera3D* _Nonnull camera;

@property (nonatomic, assign, readwrite) const simd_float2 cursorPosition;

@property (nonatomic, assign, readwrite) const NSUInteger mouseButtonMask;

@property (NS_NONATOMIC_IOSONLY, readonly) void* _Nonnull device;

@property (nonatomic, assign, readwrite) const MTLPixelFormat colorPixelFormat;

@property (nonatomic, assign, readwrite) const MTLPixelFormat depthStencilPilxelFormat;

@property (nonatomic, assign, readwrite) const MTLPrimitiveType primitiveType;

- (void) setPrimitiveType:(MTLPrimitiveType) primitiveType;

- (nonnull instancetype) initWithMTKView:(nonnull MTKView*)_view;

- (void) drawInMTKView:(nonnull MTKView*)_view;

- (void) drawableSizeWillChange:(CGSize) size;

- (void) setInstanceRows:(int) sender;

- (void) setInstanceSize:(float) sender;

- (void) setGroupScale:(float) sender;

- (void) setTextureScale:(float) scale;

- (void) updateCameras;

- (void) setMetallTextureValue:(float) sender;

- (void) setRoughnessTextureValue:(float) sender;

- (void) setBaseColorMixValue:(float) sender;

- (void) setStepPerFrameValue:(float) sender;

- (void) setSenseAngleValue:(float) sender;

- (void) setTurnSpeedValue:(float) sender;

- (void) setSenseOffsetValue:(float) sender;

- (void) setEvaporationValue:(float) sender;

- (void) setTrailWeightValue:(float) sender;

- (void) setFamilyValue:(int) sender;

@end
