#import <Foundation/Foundation.h>
#import <stdio.h>
#import <objc/runtime.h>

// Custom proxy class for testing
@interface TestProxy : NSProxy {
    id _target;
    NSString *_description;
}
- (id)initWithTarget:(id)target description:(NSString *)description;
- (NSString *)proxyDescription;
@end

@implementation TestProxy

- (id)initWithTarget:(id)target description:(NSString *)description {
    // Note: NSProxy doesn't have -init, so we don't call [super init]
    _target = [target retain];
    _description = [description retain];
    return self;
}

- (void)dealloc {
    [_target release];
    [_description release];
    [super dealloc];
}

- (NSString *)proxyDescription {
    return _description;
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel {
    // For NSProxy, we should return nil if we don't handle it directly
    // and let the runtime handle the forwarding
    if (_target) {
        return [_target methodSignatureForSelector:sel];
    }
    
    // If no target, return nil which will cause doesNotRecognizeSelector: to be called
    return nil;
}

- (void)forwardInvocation:(NSInvocation *)invocation {
    if (_target) {
        [invocation invokeWithTarget:_target];
    } else {
        [super forwardInvocation:invocation];
    }
}

- (BOOL)respondsToSelector:(SEL)sel {
    // Check our own methods first
    if (sel == @selector(proxyDescription) ||
        sel == @selector(methodSignatureForSelector:) ||
        sel == @selector(forwardInvocation:)) {
        return YES;
    }
    
    // Forward to target
    if (_target) {
        return [_target respondsToSelector:sel];
    }
    
    return NO;
}

@end

// Business logic class to be proxied
@interface BusinessObject : NSObject {
    NSString *_name;
    int _value;
}
- (id)initWithName:(NSString *)name value:(int)value;
- (NSString *)businessMethod;
- (int)getValue;
- (void)setValue:(int)value;
@end

@implementation BusinessObject

- (id)initWithName:(NSString *)name value:(int)value {
    if ((self = [super init])) {
        _name = [name retain];
        _value = value;
    }
    return self;
}

- (void)dealloc {
    [_name release];
    [super dealloc];
}

- (NSString *)businessMethod {
    return [NSString stringWithFormat:@"Business logic for %@ (value=%d)", _name, _value];
}

- (int)getValue {
    return _value;
}

- (void)setValue:(int)value {
    _value = value;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"BusinessObject(name=%@, value=%d)", _name, _value];
}

@end

// Protocol checking proxy (simpler version of NSProtocolChecker concept)
@protocol TestProtocol
- (NSString *)businessMethod;
- (int)getValue;
@end

@interface ProtocolProxy : NSProxy {
    id _target;
    Protocol *_protocol;
}
- (id)initWithTarget:(id)target protocol:(Protocol *)protocol;
@end

@implementation ProtocolProxy

- (id)initWithTarget:(id)target protocol:(Protocol *)protocol {
    _target = [target retain];
    _protocol = protocol;
    return self;
}

- (void)dealloc {
    [_target release];
    [super dealloc];
}

- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel {
    // Only allow methods in our protocol
    struct objc_method_description desc = protocol_getMethodDescription(_protocol, sel, YES, YES);
    if (desc.name != NULL) {
        return [_target methodSignatureForSelector:sel];
    }
    
    return nil;
}

- (void)forwardInvocation:(NSInvocation *)invocation {
    SEL sel = [invocation selector];
    struct objc_method_description desc = protocol_getMethodDescription(_protocol, sel, YES, YES);
    
    if (desc.name != NULL && _target) {
        [invocation invokeWithTarget:_target];
    } else {
        [super forwardInvocation:invocation];
    }
}

- (BOOL)respondsToSelector:(SEL)sel {
    struct objc_method_description desc = protocol_getMethodDescription(_protocol, sel, YES, YES);
    return (desc.name != NULL && _target && [_target respondsToSelector:sel]);
}

@end

int main(int argc, char *argv[]) {
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    
    printf("=== NSProxy Formatter Test ===\n");
    
    // Create business object
    BusinessObject *business = [[BusinessObject alloc] initWithName:@"TestBusiness" value:42];
    printf("Business object: %s\n", [[business description] UTF8String]);
    
    // Test 1: Simple custom proxy
    TestProxy *simpleProxy = [[TestProxy alloc] initWithTarget:business 
                                                   description:@"Simple test proxy"];
    printf("Simple proxy created\n");
    
    // Test proxy methods
    NSString *proxyDesc = [simpleProxy proxyDescription];
    printf("Proxy description: %s\n", [proxyDesc UTF8String]);
    
    // Test forwarded methods
    NSString *businessResult = [simpleProxy businessMethod]; // Should forward to business object
    printf("Forwarded business method: %s\n", [businessResult UTF8String]);
    
    // Test 2: Protocol-checking proxy
    ProtocolProxy *protocolProxy = [[ProtocolProxy alloc] initWithTarget:business 
                                                                protocol:@protocol(TestProtocol)];
    printf("Protocol proxy created\n");
    
    // This should work (method in protocol)
    int value = [protocolProxy getValue];
    printf("Protocol proxy getValue: %d\n", value);
    
    // Test 3: Nil target proxy (error case)
    TestProxy *nilTargetProxy = [[TestProxy alloc] initWithTarget:nil 
                                                      description:@"Broken proxy"];
    printf("Nil target proxy created\n");
    
    // Test 4: NSDistantObject simulation (we can't create real one easily)
    // This would be a real NSDistantObject in production
    printf("NSDistantObject would be tested here in full implementation\n");
    
    // Test 5: Abstract NSProxy (this would normally fail)
    // NSProxy *abstractProxy = [[NSProxy alloc] init]; // This would crash
    printf("Abstract NSProxy cannot be instantiated directly\n");
    
    // Breakpoint location for LLDB testing
    printf("Set breakpoint here for LLDB proxy formatter testing\n");
    
    // Test proxy class information
    Class proxyClass = [simpleProxy class];
    printf("Simple proxy class: %s\n", class_getName(proxyClass));
    
    Class protocolProxyClass = [protocolProxy class];
    printf("Protocol proxy class: %s\n", class_getName(protocolProxyClass));
    
    // Test proxy inheritance
    BOOL isProxy = [simpleProxy isKindOfClass:[NSProxy class]];
    printf("Simple proxy is kind of NSProxy: %s\n", isProxy ? "YES" : "NO");
    
    // Clean up
    [simpleProxy release];
    [protocolProxy release];
    [nilTargetProxy release];
    [business release];
    
    [pool release];
    return 0;
}