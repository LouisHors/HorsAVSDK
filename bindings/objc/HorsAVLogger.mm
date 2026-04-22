#import "HorsAVLogger.h"

// MARK: - HorsAVLogger Implementation

@implementation HorsAVLogger

static HorsAVLogLevel _currentLevel = HorsAVLogLevelInfo;
static BOOL _consoleLogging = YES;
static void (^_logCallback)(HorsAVLogLevel, NSString *) = nil;

+ (void)setLevel:(HorsAVLogLevel)level {
    _currentLevel = level;
}

+ (HorsAVLogLevel)currentLevel {
    return _currentLevel;
}

+ (void)enableConsoleLogging:(BOOL)enable {
    _consoleLogging = enable;
}

+ (void)setCallback:(void (^)(HorsAVLogLevel level, NSString *message))callback {
    _logCallback = [callback copy];
}

+ (void)clearCallback {
    _logCallback = nil;
}

// Internal logging method
+ (void)logWithLevel:(HorsAVLogLevel)level message:(NSString *)format, ... {
    if (level < _currentLevel) return;

    va_list args;
    va_start(args, format);
    NSString *message = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);

    if (_consoleLogging) {
        NSString *levelString = @"";
        switch (level) {
            case HorsAVLogLevelTrace: levelString = @"[TRACE]"; break;
            case HorsAVLogLevelDebug: levelString = @"[DEBUG]"; break;
            case HorsAVLogLevelInfo: levelString = @"[INFO]"; break;
            case HorsAVLogLevelWarning: levelString = @"[WARN]"; break;
            case HorsAVLogLevelError: levelString = @"[ERROR]"; break;
            default: levelString = @"[UNKNOWN]"; break;
        }
        NSLog(@"%@ %@", levelString, message);
    }

    if (_logCallback) {
        _logCallback(level, message);
    }
}

@end
