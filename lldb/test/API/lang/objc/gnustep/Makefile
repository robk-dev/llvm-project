OBJC_SOURCES := main.m

# Use GNUstep runtime with debug symbols
CFLAGS_EXTRAS := -g -O0 \
                 -fobjc-runtime=gnustep-2.1 \
                 -fblocks \
                 -fno-strict-aliasing \
                 -fexceptions \
                 -fobjc-exceptions \
                 -fconstant-string-class=NSConstantString \
                 -I/usr/local/include/GNUstep \
                 -I/usr/include/GNUstep \
                 -DGNUSTEP -DGNUSTEP_BASE_LIBRARY=1

LD_EXTRAS := -L/usr/local/lib -Wl,-rpath,/usr/local/lib \
             -lgnustep-base -lobjc -lBlocksRuntime -lpthread -lm

include Makefile.rules