///  MyMTKViewDelegate.h
///  MetalCCP
///
///  Created by Guido Schneider on 23.07.22.
#pragma once

#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>

@interface AAPLInputView : MTKView

- (BOOL)acceptsFirstResponder;

//- (BOOL)acceptsFirstMouse:(NSEvent *)event;
@end

@interface AAPLViewController : NSViewController <MTKViewDelegate>

@property (nonatomic, readwrite) BOOL isBoxDisplayed;

@property (nonatomic, readwrite) BOOL useMultisampleAntialiasing;

@property (nonatomic, readwrite) BOOL wantsExtendedDynamicRangeContent;

- (void)handleInstanceControlToggle:(NSControlStateValue)state;
@end
