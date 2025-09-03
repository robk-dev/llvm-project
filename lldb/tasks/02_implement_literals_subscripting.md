# Task 02: Implement ObjC Literals and Subscripting Support

## Problem Statement
The GNUstep runtime doesn't implement `CalculateHasNewLiteralsAndIndexing()`, which prevents Clang from enabling modern ObjC syntax like:
- Array subscripting: `arr[0]` 
- Dictionary subscripting: `dict[@"key"]`
- Modern array literals: `@[@"a", @"b"]`
- Modern dictionary literals: `@{@"key": @"value"}`

Without this, expressions using subscripting syntax fail to compile.

## Technical Background
In LLDB's expression parser, `ClangExpressionParser.cpp` calls:
```cpp
if (runtime->HasNewLiteralsAndIndexing())
    m_compiler.getLangOpts().DebuggerObjCLiteral = true;
```

This enables Clang to map subscripting syntax to the appropriate selector calls:
- `arr[i]` → `[arr objectAtIndexedSubscript:i]`
- `dict[key]` → `[dict objectForKeyedSubscript:key]`

## Research: GNUstep Subscripting Support
GNUstep Base implements the required selectors:

**NSArray:**
- `-objectAtIndexedSubscript:` (since GNUstep Base 1.24)
- `-setObject:atIndexedSubscript:` (mutable arrays)

**NSDictionary:** 
- `-objectForKeyedSubscript:` (since GNUstep Base 1.24)
- `-setObject:forKeyedSubscript:` (mutable dictionaries)

## Implementation Strategy

### Step 1: Implement Detection Method
```cpp
// In GNUstepObjCRuntime.cpp
bool GNUstepObjCRuntime::CalculateHasNewLiteralsAndIndexing() {
    if (!m_process) 
        return false;
        
    Target &target = m_process->GetTarget();
    
    // Check for subscripting method presence
    SymbolContextList sc_list;
    
    // Primary check: NSDictionary subscripting (most reliable indicator)
    target.GetImages().FindSymbolsWithNameAndType(
        ConstString("-[NSDictionary objectForKeyedSubscript:]"),
        eSymbolTypeCode, sc_list);
    
    if (!sc_list.IsEmpty()) {
        LLDB_LOG(GetLog(LLDBLog::Language), 
                 "GNUstep: Found NSDictionary subscripting support");
        return true;
    }
    
    // Fallback: NSArray subscripting
    sc_list.Clear();
    target.GetImages().FindSymbolsWithNameAndType(
        ConstString("-[NSArray objectAtIndexedSubscript:]"),
        eSymbolTypeCode, sc_list);
        
    if (!sc_list.IsEmpty()) {
        LLDB_LOG(GetLog(LLDBLog::Language), 
                 "GNUstep: Found NSArray subscripting support");
        return true;
    }
    
    // Check for GNUstep Base version symbols as indicator
    sc_list.Clear();
    target.GetImages().FindSymbolsWithNameAndType(
        ConstString("gnustep_base_version"), 
        eSymbolTypeData, sc_list);
        
    if (!sc_list.IsEmpty()) {
        // Assume modern GNUstep Base has subscripting
        LLDB_LOG(GetLog(LLDBLog::Language), 
                 "GNUstep: Found gnustep_base_version, assuming subscripting support");
        return true;
    }
    
    LLDB_LOG(GetLog(LLDBLog::Language), 
             "GNUstep: No subscripting support detected");
    return false;
}
```

### Step 2: Hook Into Runtime Detection
```cpp
// Override the base class method
bool GNUstepObjCRuntime::HasNewLiteralsAndIndexing() override {
    if (!m_checked_literals) {
        m_has_literals = CalculateHasNewLiteralsAndIndexing();
        m_checked_literals = true;
    }
    return m_has_literals;
}
```

### Step 3: Add Member Variables
```cpp
// In GNUstepObjCRuntime.h
private:
    mutable bool m_checked_literals = false;
    mutable bool m_has_literals = false;
```

### Step 4: Ensure DeclVendor Subscripting Methods
Make sure the DeclVendor includes subscripting method declarations:

```cpp
// NSArray subscripting methods
{"objectAtIndexedSubscript:", "@@:Q", true},     // -objectAtIndexedSubscript:(NSUInteger)
{"setObject:atIndexedSubscript:", "v@:@Q", true}, // -setObject:atIndexedSubscript: (mutable)

// NSDictionary subscripting methods  
{"objectForKeyedSubscript:", "@@:@", true},      // -objectForKeyedSubscript:(id)
{"setObject:forKeyedSubscript:", "v@:@@", true}, // -setObject:forKeyedSubscript: (mutable)
```

## Files to Modify
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.h`
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`
- `source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCDeclVendor.cpp`

## Testing Strategy

### Unit Tests
1. Test detection with modern GNUstep Base (should return true)
2. Test detection with older GNUstep Base (should return false)  
3. Test fallback detection mechanisms

### Integration Tests
```cpp
// Test subscripting syntax compilation
expr -l objc++ -- id arr = @[@"a", @"b"]; (id)arr[0]
expr -l objc++ -- id dict = @{@"k": @"v"}; (id)dict[@"k"] 
expr -l objc++ -- id arr = [NSArray arrayWithObjects:@"a", @"b", nil]; (id)arr[0]
```

## Success Criteria
- [ ] `CalculateHasNewLiteralsAndIndexing()` correctly detects GNUstep subscripting support
- [ ] Modern ObjC literal syntax compiles in expressions
- [ ] Subscripting expressions execute without errors
- [ ] Fallback works for older GNUstep versions
- [ ] Performance impact is minimal (cached results)

## Edge Cases to Handle
1. **Partial Subscripting**: Some classes have subscripting, others don't
2. **Version Detection**: Different GNUstep Base versions  
3. **Symbol Stripping**: Release builds may not have symbol names
4. **Custom Classes**: User classes implementing subscripting protocols

## Performance Considerations
- Cache the detection result to avoid repeated symbol lookups
- Use lightweight symbol queries
- Defer expensive checks until actually needed

## Implementation Status
- [x] Research completed
- [x] Detection method implemented
- [x] Runtime integration completed
- [x] DeclVendor methods added (already present)
- [ ] Testing completed
- [ ] Performance validated
- [ ] Ready for review

## Dependencies
- Requires Task 01 (language handling) to be completed
- Feeds into Task 06 (DeclVendor completion)
