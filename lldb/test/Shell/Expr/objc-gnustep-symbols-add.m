// REQUIRES: objc-gnustep, system-linux
//
// RUN: %build %s --compiler=clang --objc-gnustep --output=%t
// RUN: llvm-objcopy --only-keep-debug %t %t.debug
// RUN: llvm-objcopy --strip-debug %t %t.stripped
// RUN: %lldb -b -o "b stop" -o "run" \
// RUN:     -o "expr -d run-target -P2 -- (id)g_obj" \
// RUN:     -o "target symbols add --shlib %t.stripped %t.debug" \
// RUN:     -o "expr -d run-target -P2 -- (id)g_obj" \
// RUN:     -- %t.stripped | FileCheck %s

#import "objc/runtime.h"

#ifdef __has_attribute
#if __has_attribute(objc_root_class)
__attribute__((objc_root_class))
#endif
#endif
@interface NSObject {
  // Class, not id: this test is about Holder, and an `id isa` would be
  // expanded as an object in its own right.
  Class isa;
  int refcount;
}
+ (id)new;
@end
@implementation NSObject
+ (id)new {
  return class_createInstance(self, 0);
}
@end

struct Point {
  int x;
  int y;
};

@interface Holder : NSObject {
@public
  struct Point point;
}
@end
@implementation Holder
@end

id g_obj;

void stop(void) {}

int main() {
  Holder *holder = [Holder new];
  holder->point.x = 11;
  holder->point.y = 22;
  g_obj = holder;
  stop();
  return 0;
}

// The module starts without debug info, so the first look at a Holder finds
// no type for it there. `target symbols add` then supplies one without a
// module load; that has to undo the miss, or Holder keeps being described
// without the member names only debug info has.
//
// CHECK: (lldb) target symbols add
// CHECK: (lldb) expr -d run-target -P2 -- (id)g_obj
// CHECK: (Holder *) {{\$[0-9]+}} =
// CHECK:   point = {
// CHECK-NEXT:     x = 11
// CHECK-NEXT:     y = 22
