# GNUstep LLDB Expression Evaluation Debug Plan

## Problem Statement
Expression evaluation crashes when:
1. Using literal expressions (e.g., @123)
2. Calling class methods (e.g., [NSNumber numberWithInt:42])

## Root Cause Hypothesis
The crashes occur in Clang's AST lookup phase, suggesting interfaces aren't populated early enough or aren't visible to the expression parser.

## Debug Strategy

### Phase 1: Understanding the Crash
- Capture full debug logs with `log enable lldb expr/ast`
- Get complete stack traces with llvm-symbolizer
- Trace when each function in the pipeline is called

### Phase 2: Identifying Root Causes
- **Timing Issues**: When are interfaces populated vs when they're used?
- **AST Consistency**: Are methods properly formed with correct type encodings?
- **Runtime Integration**: Is GetObjectClass working correctly?

### Phase 3: Specific Crash Scenarios
- **Literal @123**: Needs NSNumber interface with +numberWithInt:
- **Class Methods**: Requires findable class with proper method list
- Both need early interface population

### Phase 4: Tools to Use
- `mcp__llvm_lldb_debug`: Step through evaluation
- `mcp__llvm_lldb_memory`: Track interface state
- `Task` agents: Deep runtime analysis
- `Bash`: Run tests with logging
- `MultiEdit`: Batch fixes

### Phase 5: Potential Fixes
1. Move EnsureMinimalFoundationInterfaces earlier
2. Fix method type encodings
3. Improve ExternalASTSource
4. Better metaclass discovery

### Phase 6: Similar Issues to Check
- Other literals: @"string", @[], @{}, @YES
- Other class methods: stringWithFormat:, arrayWithObjects:
- Property access: object.property
- Modern features: subscripting

## Implementation Priority

### Priority 1 (Immediate)
- Add comprehensive logging
- Create minimal test case
- Fix format string warnings

### Priority 2 (Short-term)
- Ensure early interface population
- Fix type encodings
- Validate AST manipulations

### Priority 3 (Medium-term)
- Implement proper ExternalASTSource
- Add caching
- Create test suite

### Priority 4 (Long-term)
- Refactor pipeline
- Performance monitoring
- Upstream patches

## Success Criteria
- @123 evaluates without crashing
- [NSNumber numberWithInt:42] works
- All literal types function
- No expression evaluation crashes
- Clean logs

## Key Files to Monitor
- GNUstepObjCDeclVendor.cpp (interface population)
- GNUstepObjCExternalASTSource (AST lookups)
- GNUstepRuntimeV2API.cpp (runtime introspection)
- GNUstepObjCRuntime.cpp (initialization)

## Test Commands
```bash
# Minimal literal test
echo "expr @123" | lldb

# Minimal class method test
echo "expr [NSNumber numberWithInt:42]" | lldb

# With logging
lldb -o "log enable lldb expr -f /tmp/expr.log" -o "expr @123"
```

## Notes
- The crash happens BEFORE our code is called
- This suggests a lookup/registration issue
- EnsureMinimalFoundationInterfaces timing is critical
- Type encodings must match runtime exactly