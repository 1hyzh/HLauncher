#import <AppKit/AppKit.h>

#include "platform/macos_url.hpp"
#include "app/launcher.hpp"

#include <string>

static Launcher* g_launcher = nullptr;
static id g_handler = nil;

@interface HLauncherURLHandler : NSObject
@end

@implementation HLauncherURLHandler

- (void)handleURLEvent:(NSAppleEventDescriptor*)event withReplyEvent:(NSAppleEventDescriptor*)replyEvent {
    (void)replyEvent;

    if (g_launcher == nullptr || event == nil) {
        return;
    }

    NSAppleEventDescriptor* directObject = [event paramDescriptorForKeyword:keyDirectObject];
    if (directObject == nil) {
        return;
    }

    const std::string absolute_url([[directObject stringValue] UTF8String]);
    if (!absolute_url.empty()) {
        g_launcher->handle_redirect_url(absolute_url);
    }
}

@end

void install_macos_url_handler(Launcher* launcher) {
    g_launcher = launcher;

    if (g_handler == nil) {
        g_handler = [HLauncherURLHandler new];
    }

    [[NSAppleEventManager sharedAppleEventManager]
        setEventHandler:g_handler
        andSelector:@selector(handleURLEvent:withReplyEvent:)
        forEventClass:kInternetEventClass
        andEventID:kAEGetURL];

    [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}
