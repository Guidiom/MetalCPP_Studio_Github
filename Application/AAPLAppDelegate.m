#import "AAPLAppDelegate.h"
#import "AAPLViewController.h"


@interface AAPLWindowController ()
@end

@implementation AAPLWindowController
{
     __weak IBOutlet NSButton * _toggleButton;
}

- (void)handleButtonState:(NSControlStateValue)state
{
    AAPLViewController*_contentView = (AAPLViewController*)self.contentViewController;
     _contentView.isBoxDisplayed = (state == NSControlStateValueOn);
}

- (void)windowDidLoad
{
     [super windowDidLoad];
    
     _toggleButton.state = NSControlStateValueOn;

     [self handleButtonState: NSControlStateValueOn];
}

- (IBAction)toggleButtonCallback:(NSButton *)sender
{
     [self handleButtonState: sender.state];
}
@end

@implementation AAPLAppDelegate


 - (void)applicationWillTerminate:(NSNotification *)aNotification {
 // Insert code here to tear down your application
 }
 
 - (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
 // Insert code here to initialize your application
 }

- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
     return YES;
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
     return YES;
}
@end
