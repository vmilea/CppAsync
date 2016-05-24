/*
* Copyright 2015-2016 Valentin Milea
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*   http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "impl/Common.h"
#include "impl/Assert.h"
#include "util/StringUtil.h"

namespace ut {

enum LogLevel
{
    LOG_None,
    LOG_Warn,
    LOG_Info,
    LOG_Debug,
    LOG_Verbose
};

namespace detail { namespace log
{
    inline LogLevel& logLevel() _ut_noexcept
    {
        static LogLevel sLogLevel = LOG_Warn;
        return sLogLevel;
    }
} }

inline LogLevel logLevel() _ut_noexcept
{
    return detail::log::logLevel();
}

inline void setLogLevel(LogLevel logLevel) _ut_noexcept
{
    detail::log::logLevel() = logLevel;
}

}

#ifdef UT_DISABLE_LOGGING

#define ut_log_warn_(    format, ...)
#define ut_log_info_(    format, ...)
#define ut_log_debug_(   format, ...)
#define ut_log_verbose_( format, ...)

#else

namespace ut {

namespace detail { namespace log
{
    static const char* PREFIXES[] = { "", "[UT-WARN] ", "[UT-INFO] ", "[UT-DEBG] ", "[UT-VERB] " };

    static const int PREFIX_LEN = 10;

    static const int LOG_BUF_SIZE = 1024;

    inline char* buffer() _ut_noexcept
    {
        static char sBuffer[LOG_BUF_SIZE];
        return sBuffer;
    }

    inline void writef(LogLevel level, const char *format, ...)
    {
        ut_dcheck(level <= logLevel());

        va_list ap;
        va_start(ap, format);

        memcpy(buffer(), PREFIXES[level], PREFIX_LEN + 1);

        vsnprintf(buffer() + PREFIX_LEN, LOG_BUF_SIZE - PREFIX_LEN, format, ap);

        printf("%s\n", buffer());

        va_end(ap);
    }
} }

}

// Lazy argument evaluation
//
#define ut_detail_write_log_(level, format, ...) \
    _ut_multi_line_macro_begin \
    if (level <= ut::logLevel()) { \
        ut::detail::log::writef(level, format, ##__VA_ARGS__); \
    } \
    _ut_multi_line_macro_end

#define ut_log_warn_(    format, ...) ut_detail_write_log_(ut::LOG_Warn,    format, ##__VA_ARGS__)
#define ut_log_info_(    format, ...) ut_detail_write_log_(ut::LOG_Info,    format, ##__VA_ARGS__)
#define ut_log_debug_(   format, ...) ut_detail_write_log_(ut::LOG_Debug,   format, ##__VA_ARGS__)
#define ut_log_verbose_( format, ...) ut_detail_write_log_(ut::LOG_Verbose, format, ##__VA_ARGS__)

#endif // UT_DISABLE_LOGGING
