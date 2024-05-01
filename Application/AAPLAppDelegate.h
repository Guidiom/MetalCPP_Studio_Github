 ///
///  AppDelegate.h
///  MetalCPP
///
///  Created by Guido Schneider on 23.07.22.
#pragma once

#import "TargetConditionals.h"

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface AAPLWindowController : NSWindowController <NSWindowDelegate>;
@end

@interface AAPLAppDelegate :  NSObject <NSApplicationDelegate>
@end

NS_ASSUME_NONNULL_END
