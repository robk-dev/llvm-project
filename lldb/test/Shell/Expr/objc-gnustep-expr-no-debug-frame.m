// REQUIRES: objc-gnustep
//
// RUN: %build %inferior_target %s --compiler=clang --objc-gnustep --output=%t

#import "objc/runtime.h"

#ifdef __has_attribute
#if __has_attribute(objc_root_class)
__attribute__((objc_root_class))
#endif
#endif
@interface NSObject {
  id isa;
  int refcount;
}
+ (id)new;
@end
@implementation NSObject
+ (id)new {
  return class_createInstance(self, 0);
}
@end

@interface Widget : NSObject
- (int)twice:(int)n;
@end
@implementation Widget
- (int)twice:(int)n { return n * 2; }
@end

Widget *g_widget;

// A frame with no debug info has no language, but expressions there are still
// compiled as Objective-C, so a message send still needs its selectors
// registered with the runtime. Without that it dispatches nowhere and the
// send returns 0.
//
// RUN: %lldb %inferior_abi -b -o "breakpoint set -n stop_here" -o "run" \
// RUN:          -o "frame info" -o "expr (int)[g_widget twice:21]" \
// RUN:          -- %t | FileCheck %s
//
// CHECK: (lldb) frame info
// CHECK-NOT: at objc-gnustep-expr-no-debug-frame.m:
// CHECK: (lldb) expr (int)[g_widget twice:21]
// CHECK: (int) $0 = 42
__attribute__((nodebug, noinline)) void stop_here(void) {
  __asm__ volatile("");
}

int main() {
  g_widget = [Widget new];
  stop_here();
  return 0;
}
