///
///  MyMTKViewDelegate.cpp
///  MetalCCP
///
///  Created by Guido Schneider on 23.07.22
///
#define WIDTH 1024
#define HEIGHT 1024

#define TARGETCOLORPIXELFORMAT MTLPixelFormatBGRA8Unorm_sRGB
#define TARGETDEPTHSTENCILFORMAT MTLPixelFormatDepth32Float_Stencil8

///Render Primitive Types
#define TRIANGELSTRIP MTLPrimitiveTypeTriangleStrip
#define LINE MTLPrimitiveTypeLine
#define TRIANGEL MTLPrimitiveTypeTriangle

#include "AAPLViewController.h"
#include "AAPLRenderAdapter.h"
#import <simd/simd.h>

/// List the keys in use within this sample
/// The enum value is the NSEvent key code

NS_OPTIONS(uint8_t, keyCode)
{
/// Keycodes that control translation
    controlsForward     = 0x0d, // W key
    controlsBackward    = 0x01, // S key
    controlsStrafeUp    = 0x0e, // E key
    controlsStrafeDown  = 0x0c, // Q key
    controlsStrafeLeft  = 0x00, // A key
    controlsStrafeRight = 0x02, // D key
    controlsStartTop    = 0x31, // Spacebar key
};

@implementation AAPLInputView
/// Opt the window into user input first responder

- (BOOL)acceptsFirstResponder { return YES; }

- (BOOL)autoResizeDrawable{ return NO; }

- (void)awakeFromNib {
    // Create a tracking area to keep track of the mouse movements and events
    NSTrackingAreaOptions options = (NSTrackingActiveAlways | NSTrackingInVisibleRect |
                                     NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved);

    NSTrackingArea *area = [[NSTrackingArea alloc] initWithRect:[self bounds] 
                                                        options:options
                                                          owner:self
                                                       userInfo:nil];
    [self addTrackingArea:area];
}

- (BOOL) acceptsFirstMouse:(NSEvent *)event { return YES; }

@end


@implementation AAPLViewController{

    AAPLInputView* _Nonnull _view;

    AAPLRenderAdapter*_Nonnull _aDapter;

    id<MTLDevice> _Nonnull _device;

/// The current key state
    NSNumber* _Nonnull _num;

/// current drag offset of mouse this frame
    simd_float2 _mouseDrag ;
    float       _wheelDelta;

    __weak IBOutlet NSBox*       _BoxContainer;
    __weak IBOutlet NSSwitch*    _boxViewEnabled;
    __weak IBOutlet NSTextField* _instanceRowsLabel;
    __weak IBOutlet NSSlider*    _instanceRowsSlider;
    __weak IBOutlet NSTextField* _instanceRowsTextField;
    __weak IBOutlet NSTextField* _instanceSizeLabel;
    __weak IBOutlet NSSlider*    _instanceSizeSlider;
    __weak IBOutlet NSTextField* _instanceSizeTextField;
    __weak IBOutlet NSTextField* _groupSizeLabel;
    __weak IBOutlet NSSlider*    _groupSizeSlider;
    __weak IBOutlet NSTextField* _groupSizeTextField;
    __weak IBOutlet NSTextField* _textureSizeLabel;
    __weak IBOutlet NSSlider*    _textureSizeSlider;
    __weak IBOutlet NSTextField* _textureSizeTextField;
    __weak IBOutlet NSTextField* _metallnessTextureLabel;
    __weak IBOutlet NSSlider*    _metallnessSlider;
    __weak IBOutlet NSTextField* _metallnessTextureTextField;
    __weak IBOutlet NSTextField* _roughnessTextureLabel;
    __weak IBOutlet NSSlider*    _roughnessSlider;
    __weak IBOutlet NSTextField* _roughnessTextureTextField;
    __weak IBOutlet NSTextField* _baseColorLabel;
    __weak IBOutlet NSTextField *_computedColorLabel;
    __weak IBOutlet NSSlider*    _baseColorMixSlider;
    __weak IBOutlet NSTextField* _baseColorMixTextField;
    __weak IBOutlet NSTextField* _gpuNameLabel;
    __weak IBOutlet NSTextField* _gpuNameTextField;
    __weak IBOutlet NSSwitch*    _primitiveSwitch;
    __weak IBOutlet NSTextField* _togglePrimitiveTextField;
    __weak IBOutlet NSTextField* _primitiveLabel;
    __weak IBOutlet NSTextField* _stepPerFrameLabel;
    __weak IBOutlet NSSlider*    _stepPerFrameSlider;
    __weak IBOutlet NSTextField* _stepPerFrameTextField;
    __weak IBOutlet NSTextField* _familyLabel;
    __weak IBOutlet NSSlider*    _familySlider;
    __weak IBOutlet NSTextField* _familyTextField;
    __weak IBOutlet NSTextField* _senseAngleLabel;
    __weak IBOutlet NSSlider*    _senseAngleSlider;
    __weak IBOutlet NSTextField* _senseAngleTextField;
    __weak IBOutlet NSTextField* _turnSpeedLabel;
    __weak IBOutlet NSSlider*    _turnSpeedSlider;
    __weak IBOutlet NSTextField* _turnSpeedTextField;
    __weak IBOutlet NSTextField* _senseOffsetLabel;
    __weak IBOutlet NSSlider*    _senseOffsetSlider;
    __weak IBOutlet NSTextField* _senseOffsetTextField;
    __weak IBOutlet NSTextField* _evaporationLabel;
    __weak IBOutlet NSSlider*    _evaporationSlider;
    __weak IBOutlet NSTextField* _evaporationTextField;
    __weak IBOutlet NSTextField* _trailWeightLabel;
    __weak IBOutlet NSSlider*    _trailWeightSlider;
    __weak IBOutlet NSTextField* _trailWeightTextField;
}


-(BOOL) _wantsExtendedDynamicRangeContent {
    return YES;
}


-(BOOL) _useMultisampleAntialiasing {
    return YES;
}

/// For capturing mouse and keyboard events
- (void)mouseExited:(NSEvent *)event         { _aDapter.cursorPosition = (simd_float2) {-1, -1 }; }
- (void)rightMouseDown:(NSEvent *)event      { _aDapter.mouseButtonMask |= 2; }
- (void)rightMouseUp:(NSEvent *)event        { _aDapter.mouseButtonMask &= (~2); }
- (void)mouseDown:(NSEvent *)event           { _aDapter.mouseButtonMask |= 1; }
- (void)mouseUp:(NSEvent *)event             { _aDapter.mouseButtonMask &= (~1); }
- (void)mouseMoved:(NSEvent *)event          { _aDapter.cursorPosition = (simd_float2){ event.locationInWindow.x,                                                                                                    _view.drawableSize.height - event.locationInWindow.y};}

- (void)mouseDragged:(NSEvent *)event        { _mouseDrag = (simd_float2){ (float)event.deltaX, (float)event.deltaY };
    float rotation_speed = 0.1f;
    [_aDapter.camera rotate:-(float) radians_from_degrees(rotation_speed * _mouseDrag.x) : _aDapter.camera.LocalUp];
    [_aDapter.camera rotate:-(float) radians_from_degrees(rotation_speed * _mouseDrag.y) : [_aDapter.camera right]];
    _mouseDrag = (simd_float2){ 0.f, 0.f };
}
- (void)scrollWheel:(NSEvent *)event {
    _wheelDelta = event.scrollingDeltaY;
    float transformation_speed = 0.1f;
    if( _wheelDelta != 0){
        vector_float3 translation = { 0.f , 0.f , 0.f };
        translation += _aDapter.camera.forward * _wheelDelta ;
        [_aDapter.camera translate:(vector_float3)translation * transformation_speed];
    }
    _wheelDelta = 0.f;
}
- (void)keyDown:(NSEvent*)event               { if(!event.ARepeat){ _num = [NSNumber numberWithUnsignedInteger: event.keyCode]; }
                                                if([_num  isEqual: @(controlsStartTop)])     {_view.paused = !_view.paused;}}
- (void)keyUp:(NSEvent*)event                 { [_num release];
                                                 _num = [NSNumber new];}

- (void)viewWillAppear
{
    [super viewWillAppear];
    
    _instanceRowsTextField.intValue = (int) (_instanceRowsSlider.intValue);
    _instanceSizeTextField.floatValue = (float) (_instanceSizeSlider.floatValue);
    _groupSizeTextField.floatValue = (float) (_groupSizeSlider.floatValue);
    _textureSizeTextField.floatValue = (float) (_textureSizeSlider.floatValue);
    _baseColorMixTextField.floatValue = (float) _baseColorMixSlider.floatValue;
    _metallnessTextureTextField.floatValue = (float)_metallnessSlider.floatValue;
    _roughnessTextureTextField.floatValue = (float) _roughnessSlider.floatValue;
    _stepPerFrameTextField.floatValue = (float) _stepPerFrameSlider.floatValue;
    _familyTextField.intValue = (int) _familySlider.intValue;
    _senseAngleTextField.floatValue = (float) _senseAngleSlider.floatValue;
    _turnSpeedTextField.floatValue = (float) _turnSpeedSlider.floatValue;
    _senseOffsetTextField.floatValue = (float) _senseOffsetSlider.floatValue;
    _evaporationTextField.floatValue = (float) _evaporationSlider.floatValue;
    _trailWeightTextField.floatValue = (float) _trailWeightSlider.floatValue;
    [_togglePrimitiveTextField setStringValue: @"TRIANGELSTRIP"];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    _device = MTLCreateSystemDefaultDevice();
    _view = (AAPLInputView*)self.view;
    
    _view.device = _device;
    
    if (![_view.device supportsFamily:MTLGPUFamilyMac2] &&
        ![_view.device supportsFamily:MTLGPUFamilyMetal3])
    {
        NSAssert( _view.device , @"Metal is not supported on this device");
        NSLog( @"Device: %@", _view.device.name );
        AAPLInputView * _Nonnull  view  = [[AAPLInputView alloc] initWithFrame: _view.frame];
        _view = view;
    }
    NSLog( @"supports MTLGPUFamilyMetal3 Device: %@", _view.device.name.description );
    _gpuNameTextField.stringValue = _view.device.name.description;
    
    [_view setDrawableSize: CGSizeMake(UINT8_C(WIDTH),UINT8_C(HEIGHT))];
    [_view setColorPixelFormat: TARGETCOLORPIXELFORMAT];
    [_view setDepthStencilPixelFormat: TARGETDEPTHSTENCILFORMAT];
    _view.preferredFramesPerSecond = 60;
    _view.clearColor  = MTLClearColorMake( 1.0, 1.0, 1.0, 1.0 );
    _view.clearDepth  = 0;
    _view.sampleCount = 1;
    
    ///initialize RenderAdapter with view
    _aDapter = [[AAPLRenderAdapter alloc] initWithMTKView: _view];
    NSAssert(_aDapter, @"Render Adapter failed initialization");
    _aDapter.colorPixelFormat = _view.colorPixelFormat;
    _aDapter.depthStencilPilxelFormat = _view.depthStencilPixelFormat;
    [_aDapter setPrimitiveType : TRIANGELSTRIP];
    [_aDapter drawableSizeWillChange: _view.bounds.size ];
    _view.delegate = self;
}


- (void)onDisplayChange
{
    _device  = MTLCreateSystemDefaultDevice();;
    if (_view.device != _device)
    {
        [self setupView:_view withDevice: _device];
        _gpuNameTextField.stringValue = _device.name;
    }
}

- (void)onGPUPlugEventWithDevice:(id<MTLDevice>)_device andNotification:(MTLDeviceNotificationName)notifier
{
    [self onDisplayChange];
}

- (void)togglePrimitiveType:(NSControlStateValue)state
{
    if (! state){
        [_aDapter setPrimitiveType: TRIANGELSTRIP ];
        [_togglePrimitiveTextField setStringValue: @"TRIANGELSTRIP"];}
        else {
            [_togglePrimitiveTextField setStringValue:@("LINESTRIP")];
            [_aDapter setPrimitiveType: TRIANGEL];
        }
}

- (void)handleInstanceControlToggle:(NSControlStateValue)state
{
    _instanceRowsSlider.enabled = state;
    _instanceRowsTextField.enabled = state;
    _instanceSizeSlider.enabled = state;
    _instanceSizeTextField.enabled = state;
    _groupSizeSlider.enabled = state;
    _groupSizeTextField.enabled = state;
    _textureSizeSlider.enabled = state;
    _textureSizeTextField.enabled = state;
    _baseColorMixSlider.enabled = state;
    _baseColorMixTextField.enabled = state;
    _metallnessSlider.enabled = state;
    _metallnessTextureTextField.enabled = state;
    _roughnessTextureTextField.enabled = state;
    _roughnessSlider.enabled = state;
    _stepPerFrameTextField.enabled = state;
    _stepPerFrameSlider.enabled = state;
    _familySlider.enabled = state;
    _familyTextField.enabled = state;
    _senseAngleSlider.enabled = state;
    _senseAngleTextField.enabled = state;
    _turnSpeedSlider.enabled = state;
    _turnSpeedTextField.enabled = state;
    _senseOffsetSlider.enabled = state;
    _senseOffsetTextField.enabled = state;
    _evaporationSlider.enabled = state;
    _evaporationTextField.enabled = state;
    _trailWeightSlider.enabled = state;
    _trailWeightTextField.enabled = state;
    _togglePrimitiveTextField.enabled = state;
    _primitiveSwitch.enabled = state;
    _gpuNameTextField.enabled = state;

    if(state)
    {
        _instanceRowsLabel.textColor = NSColor.labelColor;
        _instanceSizeLabel.textColor = NSColor.labelColor;
        _groupSizeLabel.textColor = NSColor.labelColor;
        _textureSizeLabel.textColor = NSColor.labelColor;
        _computedColorLabel.textColor = NSColor.labelColor;
        _baseColorLabel.textColor = NSColor.labelColor;
        _metallnessTextureLabel.textColor = NSColor.labelColor;
        _roughnessTextureLabel.textColor = NSColor.labelColor;
        _stepPerFrameLabel.textColor = NSColor.labelColor;
        _familyLabel.textColor = NSColor.labelColor;
        _senseAngleLabel.textColor = NSColor.labelColor;
        _turnSpeedLabel.textColor = NSColor.labelColor;
        _senseOffsetLabel.textColor = NSColor.labelColor;
        _evaporationLabel.textColor = NSColor.labelColor;
        _trailWeightLabel.textColor = NSColor.labelColor;

    } else {
        _instanceRowsLabel.textColor = NSColor.disabledControlTextColor;
        _instanceSizeLabel.textColor = NSColor.disabledControlTextColor;
        _textureSizeLabel.textColor = NSColor.disabledControlTextColor;
        _baseColorLabel.textColor = NSColor.disabledControlTextColor;
        _computedColorLabel.textColor = NSColor.disabledControlTextColor;
        _metallnessTextureLabel.textColor = NSColor.disabledControlTextColor;
        _roughnessTextureLabel.textColor = NSColor.disabledControlTextColor;
        _stepPerFrameLabel.textColor = NSColor.disabledControlTextColor;
        _familyLabel.textColor = NSColor.disabledControlTextColor;
        _senseAngleLabel.textColor = NSColor.disabledControlTextColor;
        _turnSpeedLabel.textColor = NSColor.disabledControlTextColor;
        _senseOffsetLabel.textColor = NSColor.disabledControlTextColor;
        _evaporationLabel.textColor = NSColor.disabledControlTextColor;
        _trailWeightLabel.textColor = NSColor.disabledControlTextColor;
    }
}

- (void)setupView:(nonnull MTKView *)mtkView withDevice:(id<MTLDevice>)device
{
    _view.device = device;
}

-( void )dealloc
{
    [_view release];
    [_aDapter release];
    [_BoxContainer release];
    [_num release];
    [super dealloc];
}

-(void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    assert (view == _view);
    [_aDapter drawableSizeWillChange: size];
    [_aDapter updateCameras];
    if(_view.paused)
    {
        [_view draw];
    }
}

- (void)drawInMTKView:(nonnull AAPLInputView *)_view
{
    float translation_speed = 0.1f;
    vector_float3 translation = { 0.f ,0.f ,0.f };
    if([_num  isEqual: @(controlsForward)])    {translation += _aDapter.camera.forward;}
    if([_num  isEqual: @(controlsBackward)])   {translation -= _aDapter.camera.forward;}
    if([_num  isEqual: @(controlsStrafeLeft)]) {translation -= _aDapter.camera.right;}
    if([_num  isEqual: @(controlsStrafeRight)]){translation += _aDapter.camera.right;}
    if([_num  isEqual: @(controlsStrafeUp)])   {translation += _aDapter.camera.up;}
    if([_num  isEqual: @(controlsStrafeDown)]) {translation -= _aDapter.camera.up;}
    [_aDapter.camera translate: translation_speed * translation];
    [_aDapter updateCameras];
    [_aDapter drawInMTKView: _view];
}

- (void)viewDidAppear
{
/// Make the view controller the window's first responder so that it can handle the Key events
    [_view.window makeFirstResponder:self];
}

- (IBAction)ParameterEnabledCallback:(NSSwitch *)sender
{
    [self handleInstanceControlToggle: sender.state];
}

- (IBAction)InstanceRowsSliderCallback:(NSSlider *)sender
{
    [_aDapter setInstanceRows: sender.intValue];
    _instanceRowsTextField.intValue = sender.intValue;
}

- (IBAction)InstanceSizeSliderCallback:(NSSlider *)sender
{
    [_aDapter setInstanceSize: sender.floatValue];
    _instanceSizeTextField.floatValue = sender.floatValue;
}

- (IBAction)GroupSizeSliderCallback:(NSSlider *)sender
{
    [_aDapter setGroupScale: sender.floatValue];
    _groupSizeTextField.floatValue = sender.floatValue;
}

- (IBAction)TextureSizeSliderCallback:(NSSlider *)sender
{
    [_aDapter setTextureScale: sender.floatValue];
    _textureSizeTextField.floatValue = sender.floatValue;
}

- (IBAction)MetallnessSliderCallback:(NSSlider *)sender
{
    [_aDapter setMetallTextureValue:sender.floatValue];
    _metallnessTextureTextField.floatValue = sender.floatValue;
}

- (IBAction)RoughnessSliderCallback:(NSSlider *)sender
{
    [_aDapter setRoughnessTextureValue: sender.floatValue];
    _roughnessTextureTextField.floatValue = sender.floatValue;
}

- (IBAction)BaseColorMixSliderCallback:(NSSlider *)sender
{
    [_aDapter setBaseColorMixValue: sender.floatValue];
    _baseColorMixTextField.floatValue = sender.floatValue;
}

- (IBAction)TogglePrimitiveCallBack:(NSSwitch*)sender{
    [self togglePrimitiveType: sender.state];
}

- (IBAction)StepPerFrameSliderCallback:(NSSlider *)sender
{
    [_aDapter setStepPerFrameValue: sender.floatValue];
    _stepPerFrameTextField.floatValue = sender.floatValue;
}
- (IBAction)FamilySliderCallback:(NSSlider *)sender
{
    [_aDapter setFamilyValue: sender.intValue];
    _familyTextField.intValue = sender.intValue;
}

- (IBAction)SenseAngleSliderCallback:(NSSlider *)sender
{
    [_aDapter setSenseAngleValue: sender.floatValue];
    _senseAngleTextField.floatValue = sender.floatValue;
}

- (IBAction)TurnSpeedSliderCallback:(NSSlider *)sender
{
    [_aDapter setTurnSpeedValue: sender.floatValue];
    _turnSpeedTextField.floatValue = sender.floatValue;
}

- (IBAction)SenseOffsetSliderCallback:(NSSlider *)sender
{
    [_aDapter setSenseOffsetValue: sender.floatValue];
    _senseOffsetTextField.floatValue = sender.floatValue;
}

- (IBAction)EvaporationSliderCallback:(NSSlider *)sender
{
    [_aDapter setEvaporationValue: sender.floatValue ];
    _evaporationTextField.floatValue = sender.floatValue;
}

- (IBAction)TrailWeightSliderCallback:(NSSlider *)sender
{
    [_aDapter setTrailWeightValue: sender.floatValue];
    _trailWeightTextField.floatValue = sender.floatValue;
}

- (void)setIsBoxDisplayed:(BOOL)isBoxDisplayed
{
    static const CGFloat kAnimationDuration = 0.9f;
    if (isBoxDisplayed)
    {
        __weak AAPLViewController * weakSelf = self;
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context)
         {
            context.duration = kAnimationDuration;
            AAPLViewController * strongSelf = weakSelf;
            strongSelf->_BoxContainer.contentView.superview.animator.alphaValue = 1.f;
            strongSelf->_BoxContainer.contentView.superview.hidden = NO;
        }
                            completionHandler:^
         {
            AAPLViewController * strongSelf = weakSelf;
            strongSelf->_BoxContainer.contentView.superview.alphaValue = 1.f;
        }];
    }
    else
    {
        __weak AAPLViewController * weakSelf = self;
        [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context)
         {
            context.duration = kAnimationDuration;
            AAPLViewController * strongSelf = weakSelf;
            strongSelf->_BoxContainer.contentView.superview.animator.alphaValue = 0.f;
        }
                            completionHandler:^
         {
            AAPLViewController * strongSelf = weakSelf;
            strongSelf->_BoxContainer.contentView.superview.hidden = YES;
            strongSelf->_BoxContainer.contentView.superview.alphaValue = 0.f;
        }];
    }
}

- (BOOL)commitEditingAndReturnError:(NSError **)error {
    return error;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder {

}

@end
