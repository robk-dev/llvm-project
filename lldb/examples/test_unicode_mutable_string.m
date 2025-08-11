#import <Foundation/Foundation.h>

int main(int argc, char *argv[]) {
  @autoreleasepool {
    // Test Unicode string
    NSString *unicode = @"Hello 世界 🌍";
    NSLog(@"Unicode string: %@", unicode);
    
    // Test NSMutableString 
    NSMutableString *mutable = [NSMutableString stringWithString:@"Mutable Test"];
    NSLog(@"Mutable string: %@", mutable);
    
    // Test mutable with Unicode
    NSMutableString *mutableUnicode = [NSMutableString stringWithString:@"Hello 世界 🌍"];
    [mutableUnicode appendString:@" Extra"];
    NSLog(@"Mutable Unicode: %@", mutableUnicode);
    
    // Test empty mutable
    NSMutableString *emptyMutable = [NSMutableString string];
    [emptyMutable appendString:@"Not empty anymore"];
    NSLog(@"Previously empty: %@", emptyMutable);
    
    // Set breakpoint here
    NSLog(@"All strings created"); // Breakpoint line
  }
  return 0;
}