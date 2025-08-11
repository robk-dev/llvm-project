# GNUstep Expression Evaluation Solution

## Issue Summary
LLDB's `po fruits[0]` and `po dict[@"key"]` subscript syntax doesn't work with GNUstep because:

1. GNUstep implements the older method names: `objectAtIndex:` and `objectForKey:`
2. Modern Clang expects newer method names: `objectAtIndexedSubscript:` and `objectForKeyedSubscript:`
3. LLDB's expression evaluator can't translate between the two

## Working Solutions

### For Arrays ✅
Instead of: `po fruits[0]` (doesn't work)
Use: `po [fruits objectAtIndex:0]` (works perfectly)

### For Dictionaries ✅  
Instead of: `po dict[@"key"]` (doesn't work)
Use: `po [dict objectForKey:@"key"]` (works perfectly)

### For All Other Commands ✅
These work perfectly:
- `po fruits` - shows entire array
- `po dict` - shows entire dictionary  
- `po [fruits count]` - get array size
- `po [dict allKeys]` - get all dictionary keys

## Root Cause
The subscript syntax `array[index]` is syntactic sugar that requires specific method implementations:
- Array subscripts need `objectAtIndexedSubscript:`
- Dictionary subscripts need `objectForKeyedSubscript:`

GNUstep implements the older, more standard methods which are fully compatible but use different names.

## Implementation Status
- ✅ Array/Dictionary formatters work perfectly 
- ✅ Object introspection works perfectly
- ✅ `po` commands work for objects
- ✅ Direct method calls work perfectly 
- ❌ Modern subscript syntax not yet supported (complex implementation required)

## Future Enhancement
Supporting modern subscript syntax would require:
1. Adding `objectAtIndexedSubscript:` and `objectForKeyedSubscript:` methods to DeclVendor ✅ (completed)
2. Implementing runtime method forwarding from new methods to old methods
3. OR: Implementing expression rewriting in LLDB's Clang parser
4. Comprehensive testing and integration

This is a significant undertaking that would benefit the entire GNUstep + LLDB ecosystem.