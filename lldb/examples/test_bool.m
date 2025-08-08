#import <Foundation/Foundation.h>
int main() {
    NSNumber *boolY = [NSNumber numberWithBool:YES];
    NSNumber *boolN = [NSNumber numberWithBool:NO];
    NSNumber *smallInt = [NSNumber numberWithInt:1];
    printf("boolY ptr: %p\n", boolY);
    printf("boolN ptr: %p\n", boolN);
    printf("smallInt ptr: %p\n", smallInt);
    return 0;
}
