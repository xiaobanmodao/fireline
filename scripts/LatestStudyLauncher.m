#import <Cocoa/Cocoa.h>

@interface StudyLauncher : NSObject <NSApplicationDelegate>
@property(strong) NSRunningApplication *game;
@property(strong) NSTimer *monitor;
@property(strong) NSDate *started;
@property(copy) NSString *logPath;
@end

@implementation StudyLauncher
- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    NSDictionary *config = [NSDictionary dictionaryWithContentsOfFile:
        [[NSBundle mainBundle] pathForResource:@"Study" ofType:@"plist"]];
    NSString *editor = config[@"Editor"];
    NSArray *arguments = config[@"Arguments"];
    if (!editor || !arguments.count ||
        ![[NSFileManager defaultManager] isExecutableFileAtPath:editor] ||
        ![[NSFileManager defaultManager] fileExistsAtPath:arguments[0]]) {
        NSAlert *alert = [NSAlert new];
        alert.messageText = @"找不到本机 UE5.8 或火力对决工程";
        alert.informativeText = @"请在项目中重新安装最新动作测试入口。";
        [alert runModal];
        [NSApp terminate:nil];
        return;
    }
    NSURL *bundle = [[[[NSURL fileURLWithPath:editor] URLByDeletingLastPathComponent]
                      URLByDeletingLastPathComponent] URLByDeletingLastPathComponent];
    NSWorkspaceOpenConfiguration *launch = [NSWorkspaceOpenConfiguration configuration];
    launch.arguments = arguments;
    launch.activates = YES;
    launch.createsNewApplicationInstance = NO;
    // LaunchServices otherwise silently reuses a running editor and ignores arguments.
    for (NSRunningApplication *running in [NSWorkspace sharedWorkspace].runningApplications) {
        if ([running.bundleURL.path isEqualToString:bundle.path]) {
            [running activateWithOptions:NSApplicationActivateAllWindows];
            NSAlert *alert = [NSAlert new];
            alert.messageText = @"UnrealEditor 已在运行";
            alert.informativeText = @"请先退出已有的测试窗口或编辑器，再打开此入口，以确保加载最新动作测试参数。";
            [alert runModal];
            [NSApp terminate:nil];
            return;
        }
    }
    self.logPath = [NSHomeDirectory() stringByAppendingPathComponent:
        [NSString stringWithFormat:@"Library/Logs/Fireline/StudyLaunch-%@.log", NSUUID.UUID.UUIDString]];
    [[NSFileManager defaultManager] createDirectoryAtPath:self.logPath.stringByDeletingLastPathComponent
        withIntermediateDirectories:YES attributes:nil error:nil];
    launch.arguments = [arguments arrayByAddingObject:[@"-abslog=" stringByAppendingString:self.logPath]];
    self.started = [NSDate date];
    [[NSWorkspace sharedWorkspace] openApplicationAtURL:bundle configuration:launch
        completionHandler:^(NSRunningApplication *game, NSError *error) {
            dispatch_async(dispatch_get_main_queue(), ^{
                if (error) {
                    NSAlert *alert = [NSAlert new];
                    alert.messageText = @"动作测试启动失败";
                    alert.informativeText = error.localizedDescription;
                    [alert runModal];
                    [NSApp terminate:nil];
                    return;
                }
                self.game = game;
                self.monitor = [NSTimer scheduledTimerWithTimeInterval:1 target:self
                    selector:@selector(checkLaunch:) userInfo:nil repeats:YES];
            });
        }];
}
- (void)checkLaunch:(NSTimer *)timer {
    NSString *log = [NSString stringWithContentsOfFile:self.logPath encoding:NSUTF8StringEncoding error:nil];
    BOOL failed = [log containsString:@"Failed to enter /Game/"];
    if (failed || self.game.terminated || -self.started.timeIntervalSinceNow > 180) {
        [timer invalidate];
        [NSApp activateIgnoringOtherApps:YES];
        NSAlert *alert = [NSAlert new];
        alert.messageText = failed ? @"训练场地图未能加载" : @"测试版未完成启动";
        alert.informativeText = [NSString stringWithFormat:
            @"如果日志出现 Error opening file，请检查系统设置 → 隐私与安全性 → 文件与文件夹 → UnrealEditor 的文稿文件夹权限。游戏工程位于文稿目录。\n\n启动日志：%@", self.logPath];
        [alert runModal];
        [NSApp terminate:nil];
    } else if ([log containsString:@"Bringing World /Game/Fireline/Maps/FirelineRange.FirelineRange up for play"] &&
               [log containsString:@"LoadMap:"] && [log containsString:@"Took"]) {
        [timer invalidate];
        [self.game activateWithOptions:NSApplicationActivateAllWindows];
        [NSApp terminate:nil];
    }
}
@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        StudyLauncher *delegate = [StudyLauncher new];
        app.delegate = delegate;
        [app run];
    }
    return 0;
}
