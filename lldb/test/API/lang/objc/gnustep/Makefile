OBJC_SOURCES := main.m

# Use GNUstep runtime with debug symbols - comprehensive test configuration
CFLAGS_EXTRAS := -g -gdwarf-5 -O0 \
                 -fobjc-runtime=gnustep-2.1 \
                 -fblocks \
                 -fno-strict-aliasing \
                 -fexceptions \
                 -fobjc-exceptions \
                 -fno-omit-frame-pointer \
                 -fconstant-string-class=NSConstantString \
                 -I/usr/local/include/GNUstep \
                 -I/usr/local/include \
                 -I/usr/include/GNUstep \
                 -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1 -DDEBUG=1

LD_EXTRAS := -L/usr/local/lib -Wl,-rpath,/usr/local/lib \
             -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm -ldl

include Makefile.rules