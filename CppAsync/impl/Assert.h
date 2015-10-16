/*
* Copyright 2015 Valentin Milea
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

#include "Common.h"
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>

//
// Checking internal consistency (only in DEBUG builds)
//

#ifdef NDEBUG

#define ut_assert(condition)
#define ut_assertf(condition, format, ...)

#else

#define ut_assert(condition) \
    _ut_multi_line_macro_begin \
    if (!(condition)) { \
        fputs("[CPP-ASYNC] ASSERT FAILED: " #condition "\n", stderr); \
        std::terminate(); \
    } \
    _ut_multi_line_macro_end

#define ut_assertf(condition, format, ...) \
    _ut_multi_line_macro_begin \
    if (!(condition)) { \
        fprintf(stderr, "[CPP-ASYNC] ASSERT FAILED: " #condition  " --- " format "\n", \
            ##__VA_ARGS__); \
        std::terminate(); \
    } \
    _ut_multi_line_macro_end

#endif

//
// Checking public contract (only in DEBUG builds)
//

#ifdef NDEBUG

#define ut_dcheck(condition)
#define ut_dcheckf(condition, format, ...)

#else

#define ut_dcheck(condition) \
    ut_check(condition)

#define ut_dcheckf(condition, format, ...) \
    ut_checkf(condition, format, ##__VA_ARGS__)

#endif

//
// Checking public contract (in DEBUG and RELEASE)
//

#define ut_check(condition) \
    _ut_multi_line_macro_begin \
    if (!(condition)) { \
        fputs("[CPP-ASYNC] CHECK FAILED: " #condition "\n", stderr); \
        std::terminate(); \
    } \
    _ut_multi_line_macro_end

#define ut_checkf(condition, format, ...) \
    _ut_multi_line_macro_begin \
    if (!(condition)) { \
        fprintf(stderr, "[CPP-ASYNC] CHECK FAILED: " #condition  " --- " format "\n", \
            ##__VA_ARGS__); \
        std::terminate(); \
    } \
    _ut_multi_line_macro_end

#include "../util/StringUtil.h"
