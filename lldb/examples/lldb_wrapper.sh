#!/bin/bash
# LLDB wrapper script with proper environment for GNUstep plugin

# Set environment for minimal Python issues
export PYTHONHOME=/c/tools/msys64/ucrt64
export PYTHONPATH=/c/tools/msys64/ucrt64/lib/python3.12:/c/tools/msys64/ucrt64/lib/python3.12/lib-dynload:/c/tools/msys64/ucrt64/lib/python3.12/site-packages
export PATH=/ucrt64/lib:/ucrt64/bin:$PATH

# Try to work around threading issues
export PTHREAD_MUTEX_ROBUST=0
export OMP_NUM_THREADS=1

# Launch LLDB with proper environment and console handling
exec winpty /c/Users/vagrant/code/llvm-project/build/bin/lldb.exe "$@"
