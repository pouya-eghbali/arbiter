#import <UIKit/UIKit.h>
#include <QtGlobal>

qreal arbiterIOSFontScale() {
    __block CGFloat scale = 1;
    auto read = ^{ scale = [[UIFontMetrics metricsForTextStyle:UIFontTextStyleBody] scaledValueForValue:17] / 17.; };
    if (NSThread.isMainThread) read();
    else dispatch_sync(dispatch_get_main_queue(), read);
    return scale;
}
