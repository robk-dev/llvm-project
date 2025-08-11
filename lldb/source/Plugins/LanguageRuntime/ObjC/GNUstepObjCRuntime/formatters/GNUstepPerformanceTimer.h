//===-- GNUstepPerformanceTimer.h ------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_PERFORMANCE_TIMER_H
#define LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_PERFORMANCE_TIMER_H

#ifdef GNUSTEP_FORMATTER_PERFORMANCE_MEASUREMENT

#include <chrono>
#include <string>
#include "lldb/Utility/Log.h"

namespace lldb_private {
namespace formatters {

/// Lightweight performance timer for GNUstep formatters
/// Only active when GNUSTEP_FORMATTER_PERFORMANCE_MEASUREMENT is defined
class GNUstepFormatterTimer {
public:
    explicit GNUstepFormatterTimer(const char* formatter_name) 
        : m_formatter_name(formatter_name),
          m_start_time(std::chrono::high_resolution_clock::now()) {}
    
    ~GNUstepFormatterTimer() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - m_start_time);
        
        // Log slow formatters (>50ms = 50,000 microseconds)
        if (duration.count() > 50000) {
            if (Log *log = GetLog(LLDBLog::DataFormatters)) {
                log->Printf("PERFORMANCE WARNING: %s formatter took %lld μs (%.2f ms)", 
                           m_formatter_name, 
                           static_cast<long long>(duration.count()),
                           duration.count() / 1000.0);
            }
        }
        
        // Optional: Log all formatters for detailed analysis
        #ifdef GNUSTEP_FORMATTER_PERFORMANCE_VERBOSE
        if (Log *log = GetLog(LLDBLog::DataFormatters)) {
            log->Printf("PERFORMANCE: %s formatter: %lld μs", 
                       m_formatter_name, 
                       static_cast<long long>(duration.count()));
        }
        #endif
    }

private:
    const char* m_formatter_name;
    std::chrono::high_resolution_clock::time_point m_start_time;
};

#define GNUSTEP_PERFORMANCE_TIMER(name) \
    GNUstepFormatterTimer timer_(name)

} // namespace formatters
} // namespace lldb_private

#else

// No-op macros when performance measurement is disabled
#define GNUSTEP_PERFORMANCE_TIMER(name) ((void)0)

#endif // GNUSTEP_FORMATTER_PERFORMANCE_MEASUREMENT

#endif // LLDB_SOURCE_PLUGINS_LANGUAGERUNTIME_OBJC_GNUSTEPOBJCRUNTIME_FORMATTERS_PERFORMANCE_TIMER_H