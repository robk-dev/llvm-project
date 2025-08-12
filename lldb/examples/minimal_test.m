// Minimal Objective-C test without Foundation to avoid header conflicts
#include <stdio.h>
#include <stdlib.h>
#include <objc/runtime.h>

@interface TestClass
@property (nonatomic, assign) int testProperty;
- (void)testMethod;
+ (instancetype)new;
@end

@implementation TestClass
- (void)testMethod {
    printf("TestClass method called\n");
}

+ (instancetype)new {
    return class_createInstance(self, 0);
}
@end

int main(int argc, const char *argv[]) {
    TestClass *obj = [TestClass new];
    [obj testMethod];
    
    // Test runtime introspection
    unsigned int methodCount = 0;
    Method *methods = class_copyMethodList([TestClass class], &methodCount);
    printf("TestClass has %u methods\n", methodCount);
    
    if (methods) {
        free(methods);
    }
    return 0;
}
