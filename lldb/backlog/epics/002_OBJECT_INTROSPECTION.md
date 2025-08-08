# Epic 002: Object Introspection & Type System

## Overview
Develop comprehensive object introspection capabilities for GNUstep Objective-C objects, enabling LLDB to understand object layouts, instance variables, method tables, and protocol conformance.

## Business Value
- **Foundation for advanced debugging**: Enables all higher-level debugging features
- **Object exploration**: Developers can inspect object internals and relationships
- **Type accuracy**: Improves debugging precision with correct type information
- **Memory debugging**: Better understanding of object layouts aids memory issue diagnosis

## Current Status: 20% Complete

### ✅ Completed
- Basic introspector framework exists in `GNUstepObjCRuntimeIntrospector.cpp`
- Class name resolution interface defined
- Integration with main runtime plugin established

### 🔄 In Progress  
- ISA resolution implementation (see Epic 001, Task 02)
- Basic object structure understanding

### 📋 Planned
- Instance variable enumeration and access
- Method table introspection
- Protocol conformance detection
- Property access and metadata
- Category and extension handling

## Technical Scope

### Core Capabilities
1. **Object Structure Analysis** - Understanding GNUstep object memory layouts
2. **Instance Variable Access** - Reading and interpreting object member data
3. **Method Introspection** - Accessing method tables and signatures
4. **Protocol Conformance** - Detecting implemented protocols
5. **Property Metadata** - Understanding property attributes and accessors
6. **Runtime Metadata** - Accessing class, category, and extension information

### Key Files
- **Core Introspector**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.cpp`
- **Header Definitions**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntimeIntrospector.h`
- **Integration Point**: `/home/robk/code/llvm-project/lldb/source/Plugins/LanguageRuntime/ObjC/GNUstepObjCRuntime/GNUstepObjCRuntime.cpp`

## Dependencies
- **Epic 001**: Core runtime foundation (ISA resolution)
- **GNUstep Runtime**: Deep understanding of libobjc2 structures
- **LLDB APIs**: ValueObject and Type system integration

## Acceptance Criteria

### Must Have
1. **Instance Variable Enumeration**
   - List all instance variables for any object
   - Access variable names, types, and offsets
   - Read variable values safely from object memory

2. **Method Table Access**
   - Enumerate available methods for a class
   - Extract method signatures and implementations
   - Handle inherited methods from superclasses

3. **Type Information**
   - Accurate type information for all object components
   - Integration with LLDB's type system
   - Support for complex types (structs, unions, pointers)

### Should Have
1. **Protocol Support**
   - Detect protocol conformance
   - Access protocol method requirements
   - Handle optional protocol methods

2. **Property Introspection**
   - Enumerate declared properties
   - Access property attributes (readonly, atomic, etc.)
   - Link properties to underlying instance variables

### Could Have
1. **Advanced Features**
   - Category method detection
   - Associated object access
   - Runtime method addition detection

---

*Epic Owner*: Development Team  
*Created*: December 2024  
*Target Completion*: Q1-Q2 2025
