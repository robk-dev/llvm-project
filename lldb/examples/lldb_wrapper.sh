#!/bin/bash
# LLDB wrapper script with proper environment for GNUstep plugin

export PYTHONHOME=/ucrt64
export PYTHONPATH=/ucrt64/lib/python3.12:/ucrt64/lib/python3.12/site-packages
export PATH=/ucrt64/lib:/ucrt64/bin:$PATH

# Try to work around threading issues
export PTHREAD_MUTEX_ROBUST=0
export OMP_NUM_THREADS=1

# Launch our LLDB with GNUstep plugin
exec c:/tools/msys64/home/kardjali/code/llvm-project/build/bin/lldb.exe "$@"
