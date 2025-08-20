# Makefile for LLDB Bridge Examples - Debug Symbol Edition

# Include Conan build info for GNUstep paths
include conanbuildinfo.mak

# Use system clang 20.1.8 (compatible with current environment)
CC = clang
LLDB = ../build/bin/lldb

# WORKING GNUstep compilation flags - CRITICAL: Use gnustep-2.0 runtime and LLD linker!
# These flags were discovered through testing and are required for proper method dispatch
CFLAGS = -MMD -MP \
         -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DGNU_GUI_LIBRARY=1 \
         -DGNUSTEP_RUNTIME=1 -D_NONFRAGILE_ABI=1 -DGNUSTEP_WITH_DLL \
         -fno-strict-aliasing -fexceptions -fobjc-exceptions \
         -D_NATIVE_OBJC_EXCEPTIONS -fno-omit-frame-pointer \
         -DGSWARN -DGSDIAGNOSE -Wno-import \
         -g -O0 -fobjc-runtime=gnustep-2.0 -fblocks \
         -fconstant-string-class=NSConstantString \
         -IC:/.conan/24ea94/1/include \
         -IC:/.conan/24ea94/1/GNUstep/System/Library/Headers

LDFLAGS = -Wl,--enable-auto-import -fuse-ld=lld -lstdc++ -lgcc_s \
          -LC:/.conan/24ea94/1/lib \
          -LC:/.conan/24ea94/1/GNUstep/System/Library/Libraries

LIBS = -lgnustep-base -lobjc -lws2_32 -ladvapi32 -lcomctl32 \
       -luser32 -lcomdlg32 -lmpr -lnetapi32 -lm -lpthread

# Source files
SOURCES = simple_test.m custom_class_test.m foundation_test.m string_test.m \
          test_nsstring_formatter.m test_nsnumber_formatter.m array_test.m \
          simple_array_test.m test_nsnumber_comprehensive.m \
          test_nsarray_comprehensive.m dictionary_test.m nsset_test.m \
          test_collections_formatter.m test_method_enum.m test_indexset.m \
          test_decimalnumber.m test_characterset.m test_data_url_uuid.m \
          test_userdefaults.m test_tagged_pointers.m test_stepping_trampoline.m \
          test_object_checker.m test_exception_resolver.m

# Build targets
TARGETS = $(SOURCES:.m=)

all: $(TARGETS)

%: %.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

simple_test: simple_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

custom_class_test: custom_class_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

foundation_test: foundation_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

string_test: string_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_nsstring_formatter: test_nsstring_formatter.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_nsnumber_formatter: test_nsnumber_formatter.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

array_test: array_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

simple_array_test: simple_array_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_nsnumber_comprehensive: test_nsnumber_comprehensive.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_nsarray_comprehensive: test_nsarray_comprehensive.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

dictionary_test: dictionary_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

nsset_test: nsset_test.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_collections_formatter: test_collections_formatter.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_method_enum: test_method_enum.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_indexset: test_indexset.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_decimalnumber: test_decimalnumber.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_characterset: test_characterset.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_data_url_uuid: test_data_url_uuid.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_userdefaults: test_userdefaults.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_tagged_pointers: test_tagged_pointers.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_stepping_trampoline: test_stepping_trampoline.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_object_checker: test_object_checker.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

test_exception_resolver: test_exception_resolver.m
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f $(TARGETS) *.d

.PHONY: all clean
