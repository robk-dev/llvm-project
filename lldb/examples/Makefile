# Makefile for LLDB Bridge Examples - Debug Symbol Edition

# Use the LLVM build clang for consistency
CC = clang
LLDB = /home/robk/code/llvm-project/build/bin/lldb

# Enhanced compilation flags for better debugging experience
CFLAGS = -fobjc-runtime=gnustep-2.1 \
         -fblocks \
         -fno-strict-aliasing \
         -fexceptions \
         -fobjc-exceptions \
         -g -gdwarf-5 -O0 \
         -fno-omit-frame-pointer \
         -I/home/robk/code/llvm-project/lldb/gnustep-install/include \
         -I/home/robk/code/llvm-project/lldb/gnustep-install/include/GNUstepBase \
         -I/home/robk/code/llvm-project/lldb/gnustep-install/include/Foundation \
         -I/usr/local/include/GNUstep \
         -I/usr/include/GNUstep \
         -fconstant-string-class=NSConstantString \
         -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DDEBUG=1 \
         -Wno-error=int-conversion

LDFLAGS = -L/home/robk/code/llvm-project/lldb/gnustep-install/lib \
          -Wl,-rpath,/home/robk/code/llvm-project/lldb/gnustep-install/lib \
          -L/usr/local/lib \
          -Wl,-rpath,/usr/local/lib \
          -g

LIBS = -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm

# Source files
SOURCES = simple_test.m custom_class_test.m foundation_test.m string_test.m \
          test_nsstring_formatter.m test_nsnumber_formatter.m array_test.m \
          simple_array_test.m test_nsnumber_comprehensive.m test_nsarray_comprehensive.m \
          dictionary_test.m nsset_test.m test_collections_formatter.m test_method_enum.m \
          test_indexset.m test_decimalnumber.m test_characterset.m test_data_url_uuid.m \
          test_userdefaults.m test_tagged_pointers.m test_stepping_trampoline.m \
          test_object_checker.m test_exception_resolver.m
EXECUTABLES = $(SOURCES:.m=)

.PHONY: all clean clean-all symbols help debug debug-simple debug-custom debug-foundation debug-nsstring debug-nsnumber debug-array debug-dictionary debug-set debug-collections debug-method-enum debug-data-url-uuid debug-timezone debug-timezone-comprehensive test-plugin

all: $(EXECUTABLES)

%: %.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(EXECUTABLES)

# Enhanced clean - removes all build artifacts and temporary files
clean-all: clean
	@echo "Performing comprehensive cleanup..."
	@find . -maxdepth 1 -type f -executable -not -name "*.sh" -not -name "*.py" -not -name "*.lldb" -delete 2>/dev/null || true
	@find . -name "core.*" -delete 2>/dev/null || true
	@find . -name "*.dSYM" -type d -exec rm -rf {} + 2>/dev/null || true  
	@find . -name "*.o" -delete 2>/dev/null || true
	@find . -name "a.out" -delete 2>/dev/null || true
	@find . -name "*.tmp" -delete 2>/dev/null || true
	@echo "Comprehensive cleanup completed"

# Target to check symbols in generated binaries
symbols: custom_class_test
	@echo "Checking for ivar offset symbols in custom_class_test..."
	@objdump -t custom_class_test | grep "__objc_ivar_offset" || echo "No ivar offset symbols found"
	@echo ""
	@echo "Checking for debug symbols..."
	@objdump -h custom_class_test | grep debug && echo "Debug symbols present" || echo "No debug symbols found"
	@echo ""
	@echo "Checking library dependencies..."
	@ldd custom_class_test | grep -E "(gnustep|objc)"

# Individual targets for convenience
simple_test: simple_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

custom_class_test: custom_class_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

foundation_test: foundation_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_nsstring_formatter: test_nsstring_formatter.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

# Debug targets for testing our GNUstep runtime plugin
debug: custom_class_test
	@echo "Starting LLDB with custom_class_test..."
	@export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) custom_class_test

debug-simple: simple_test
	@echo "Starting LLDB with simple_test..."
	@export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) simple_test

debug-custom: custom_class_test
	$(LLDB) ./custom_class_test

debug-foundation: foundation_test
	$(LLDB) ./foundation_test

debug-nsstring: test_nsstring_formatter
	@echo "Starting LLDB with NSString formatter test..."
	@echo "Commands to test the formatter:"
	@echo "  (lldb) b test_nsstring_formatter.m:69"
	@echo "  (lldb) run"
	@echo "  (lldb) po constantString1"
	@echo "  (lldb) po mutableString1"
	@echo "  (lldb) po utf8String"
	@echo "  (lldb) po unicode"
	@echo "  (lldb) p constantString1"
	@echo "  (lldb) frame variable -O"
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./test_nsstring_formatter

debug-nsnumber: test_nsnumber_formatter
	@echo "Starting LLDB with NSNumber formatter test..."
	@echo "Commands to test the formatter:"
	@echo "  (lldb) b test_nsnumber_formatter.m:49"
	@echo "  (lldb) run"
	@echo "  (lldb) po intNumber"
	@echo "  (lldb) po boolYes"
	@echo "  (lldb) po boolNo"
	@echo "  (lldb) po floatNumber"
	@echo "  (lldb) po doubleNumber"
	@echo "  (lldb) po longLongNumber"
	@echo "  (lldb) po unsignedLongLong"
	@echo "  (lldb) po smallInt1"
	@echo "  (lldb) frame variable -O"
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./test_nsnumber_formatter

debug-array: array_test
	@echo "Starting LLDB with NSArray formatter test..."
	@echo "Commands to test the formatter:"
	@echo "  (lldb) b array_test.m:97"
	@echo "  (lldb) run"
	@echo "  (lldb) po emptyArray"
	@echo "  (lldb) po stringArray"
	@echo "  (lldb) po numberArray"
	@echo "  (lldb) po objectArray"
	@echo "  (lldb) po nestedArray"
	@echo "  (lldb) po mutableArray"
	@echo "  (lldb) po largeArray"
	@echo "  (lldb) po mixedArray"
	@echo "  (lldb) frame variable -O"
	@echo "  (lldb) frame variable stringArray"
	@echo "  (lldb) p stringArray[0]"
	@echo "  (lldb) p stringArray[1]"
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./array_test

# Test our plugin with a quick debug session
test-plugin: custom_class_test
	@echo "Testing GNUstep ObjectiveC Runtime V2 Plugin..."
	@echo "Commands to try in LLDB:"
	@echo "  (lldb) b custom_class_test.m:122"
	@echo "  (lldb) run"
	@echo "  (lldb) po account"
	@echo "  (lldb) po account.accountNumber"
	@echo "  (lldb) po accountSummary"
	$(LLDB) ./custom_class_test

# Duplicate debug targets removed - using the first definitions above

# Additional test targets
test_nsnumber_comprehensive: test_nsnumber_comprehensive.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_nsarray_comprehensive: test_nsarray_comprehensive.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

simple_array_test: simple_array_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

dictionary_test: dictionary_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

nsset_test: nsset_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_collections_formatter: test_collections_formatter.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

debug-dictionary: dictionary_test
	@echo "Starting LLDB with NSDictionary formatter test..."
	@echo "Commands to test the formatter:"
	@echo "  (lldb) b dictionary_test.m:53"
	@echo "  (lldb) run"
	@echo "  (lldb) po emptyDict"
	@echo "  (lldb) po singleDict"
	@echo "  (lldb) po multiDict"
	@echo "  (lldb) po nestedDict"
	@echo "  (lldb) po mutableDict"
	@echo "  (lldb) po mixedDict"
	@echo "  (lldb) po largeDict"
	@echo "  (lldb) frame variable -O"
	@echo "  (lldb) frame variable multiDict"
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./dictionary_test

debug-set: nsset_test
	@echo "Starting LLDB with NSSet formatter test..."
	@echo "Commands to test the formatter:"
	@echo "  (lldb) b nsset_test.m:115"
	@echo "  (lldb) run"
	@echo "  (lldb) po emptySet"
	@echo "  (lldb) po singleSet"
	@echo "  (lldb) po stringSet"
	@echo "  (lldb) po numberSet"
	@echo "  (lldb) po mixedSet"
	@echo "  (lldb) po mutableSet"
	@echo "  (lldb) po customSet"
	@echo "  (lldb) po largeSet"
	@echo "  (lldb) po countedSet"
	@echo "  (lldb) po nestedSet"
	@echo "  (lldb) frame variable -O"
	@echo "  (lldb) frame variable stringSet"
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./nsset_test

debug-collections: test_collections_formatter
	@echo "Starting LLDB with comprehensive collections formatter test..."
	@echo "This test validates NSDictionary and NSSet formatters"
	@echo ""
	@echo "Key breakpoints and tests:"
	@echo "  (lldb) b test_collections_formatter.m:51   # Empty dictionary"
	@echo "  (lldb) b test_collections_formatter.m:56   # Single element dict"
	@echo "  (lldb) b test_collections_formatter.m:65   # Multi-element dict"
	@echo "  (lldb) b test_collections_formatter.m:80   # Mixed types dict"
	@echo "  (lldb) b test_collections_formatter.m:90   # Nested dict"
	@echo "  (lldb) b test_collections_formatter.m:100  # Custom objects dict"
	@echo "  (lldb) b test_collections_formatter.m:110  # Large dict (100 items)"
	@echo "  (lldb) b test_collections_formatter.m:121  # Mutable dict"
	@echo "  (lldb) b test_collections_formatter.m:129  # Empty set"
	@echo "  (lldb) b test_collections_formatter.m:134  # Single element set"
	@echo "  (lldb) b test_collections_formatter.m:139  # Multi-element set"
	@echo "  (lldb) b test_collections_formatter.m:151  # Mixed types set"
	@echo "  (lldb) b test_collections_formatter.m:159  # Set with collections"
	@echo "  (lldb) b test_collections_formatter.m:167  # Custom objects set"
	@echo "  (lldb) b test_collections_formatter.m:176  # Large set (150 items)"
	@echo "  (lldb) b test_collections_formatter.m:187  # Mutable set"
	@echo "  (lldb) b test_collections_formatter.m:218  # Complex nested structure"
	@echo "  (lldb) b test_collections_formatter.m:223  # Nil dictionary"
	@echo "  (lldb) b test_collections_formatter.m:227  # Nil set"
	@echo "  (lldb) b test_collections_formatter.m:236  # Long strings dict"
	@echo "  (lldb) b test_collections_formatter.m:241  # Final inspection"
	@echo ""
	@echo "Quick test sequence:"
	@echo "  (lldb) b main"
	@echo "  (lldb) run"
	@echo "  (lldb) continue   # Multiple times to step through tests"
	@echo "  (lldb) po emptyDict"
	@echo "  (lldb) po multiDict"
	@echo "  (lldb) po complexStructure"
	@echo ""
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./test_collections_formatter

test_method_enum: test_method_enum.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

debug-method-enum: test_method_enum
	@echo "Starting LLDB with method enumeration test..."
	@echo "Commands to test method introspection:"
	@echo "  (lldb) b test_method_enum.m:110"
	@echo "  (lldb) run"
	@echo "  (lldb) script"
	@echo "  >>> import lldb"
	@echo "  >>> obj = lldb.frame.FindVariable('testObj')"
	@echo "  >>> cls = obj.GetChildMemberWithName('isa')"
	@echo "  >>> # Method introspection will be available here"
	@echo "  (lldb) po testObj"
	@echo "  (lldb) po subObj"
	@echo "  (lldb) frame variable -O"
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./test_method_enum

test_data_url_uuid: test_data_url_uuid.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

debug-data-url-uuid: test_data_url_uuid
	@echo "Starting LLDB with NSData, NSURL, and NSUUID formatter test..."
	@echo "Commands to test the formatters:"
	@echo "  (lldb) b test_data_url_uuid.m:48   # NSData objects breakpoint"
	@echo "  (lldb) b test_data_url_uuid.m:84   # NSURL objects breakpoint"
	@echo "  (lldb) b test_data_url_uuid.m:114  # NSUUID objects breakpoint"
	@echo "  (lldb) b test_data_url_uuid.m:127  # Final comprehensive test"
	@echo "  (lldb) run"
	@echo "  (lldb) po emptyData"
	@echo "  (lldb) po smallData"
	@echo "  (lldb) po binaryData"
	@echo "  (lldb) po largeData"
	@echo "  (lldb) po mutableData"
	@echo "  (lldb) continue  # Move to NSURL tests"
	@echo "  (lldb) po fileURL"
	@echo "  (lldb) po httpURL"
	@echo "  (lldb) po complexHttpURL"
	@echo "  (lldb) continue  # Move to NSUUID tests"
	@echo "  (lldb) po randomUUID"
	@echo "  (lldb) po stringUUID"
	@echo "  (lldb) po zeroUUID"
	@echo "  (lldb) frame variable -O"
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./test_data_url_uuid

test_timezone_comprehensive: test_timezone_comprehensive.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

debug-timezone: test_timezone_basic
	@echo "Starting LLDB with NSTimeZone enhanced formatter test..."
	@echo "This tests the enhanced timezone debugging capabilities added to LLDB"
	@echo ""
	@echo "Key improvements being tested:"
	@echo "  ✓ Complex timezones (America/New_York, Asia/Tokyo) show names instead of raw memory"
	@echo "  ✓ NSLocalTimeZone shows meaningful timezone information" 
	@echo "  ✓ GSAbsTimeZone shows clean timezone names and offsets"
	@echo "  ✓ GSTimeZone shows timezone names with DST information"
	@echo ""
	@echo "Commands to test:"
	@echo "  (lldb) b test_timezone_basic.m:19"
	@echo "  (lldb) run"
	@echo "  (lldb) po nyTimeZone        # Should show: GSTimeZone(name=\"America/New_York\", ...)"
	@echo "  (lldb) po tokyoTimeZone     # Should show: GSTimeZone(name=\"Asia/Tokyo\", ...)"
	@echo "  (lldb) po localTimeZone     # Should show: NSLocalTimeZone(name=\"...\", ...)"
	@echo "  (lldb) po offsetTimeZone    # Should show: GSAbsTimeZone(name=\"GMT+0100\", offset=3600)"
	@echo ""
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./test_timezone_basic

debug-timezone-comprehensive: test_timezone_comprehensive
	@echo "Starting LLDB with comprehensive NSTimeZone formatter test..."
	@echo "This is the comprehensive test of the enhanced NSTimeZone formatter"
	@echo ""
	@echo "Tests all timezone types:"
	@echo "  • GSAbsTimeZone (simple fixed offsets)"
	@echo "  • GSTimeZone (complex zoneinfo-based timezones)"  
	@echo "  • NSLocalTimeZone (system timezone proxy)"
	@echo "  • Edge cases (nil, invalid timezones)"
	@echo ""
	@echo "Quick test commands:"
	@echo "  (lldb) b test_timezone_comprehensive.m:41"
	@echo "  (lldb) run"
	@echo "  (lldb) po newYorkTZ"
	@echo "  (lldb) po tokyoTZ"
	@echo "  (lldb) po localTZ"
	@echo ""
	export PATH="$(dir $(LLDB)):$$PATH" && \
	export LLDB_DEBUGSERVER_PATH="$(dir $(LLDB))lldb-server" && \
	$(LLDB) ./test_timezone_comprehensive

# Help target
help:
	@echo "Available targets:"
	@echo "  all                    - Build all examples"
	@echo "  simple_test            - Build simple test example"
	@echo "  custom_class_test      - Build custom class example"
	@echo "  foundation_test        - Build Foundation types example"
	@echo "  test_nsstring_formatter - Build NSString formatter test"
	@echo "  test_nsnumber_formatter - Build NSNumber formatter test"
	@echo "  test_nsnumber_comprehensive - Build comprehensive NSNumber test"
	@echo "  test_nsarray_comprehensive - Build comprehensive NSArray test"
	@echo "  array_test             - Build NSArray formatter test"
	@echo "  simple_array_test      - Build simple NSArray test"
	@echo "  test_collections_formatter - Build comprehensive collections formatter test"
	@echo "  symbols                - Check symbols in custom_class_test"
	@echo "  debug-simple           - Debug simple_test with LLDB"
	@echo "  debug-custom           - Debug custom_class_test with LLDB"
	@echo "  debug-foundation       - Debug foundation_test with LLDB"
	@echo "  debug-nsstring         - Debug NSString formatter test with LLDB"
	@echo "  debug-nsnumber         - Debug NSNumber formatter test with LLDB"
	@echo "  debug-array            - Debug NSArray formatter test with LLDB"
	@echo "  debug-dictionary       - Debug NSDictionary formatter test with LLDB"
	@echo "  debug-set              - Debug NSSet formatter test with LLDB"
	@echo "  debug-collections      - Debug comprehensive collections formatter test with LLDB"
	@echo "  test-plugin            - Test our GNUstep plugin with guided commands"
	@echo "  clean                  - Remove all built executables"
	@echo "  help                   - Show this help message"
