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

#include "../Common.h"
#include "Thread.h"
#include <cstdint>

namespace util {

using Timepoint = util::chrono::time_point<
    util::chrono::high_resolution_clock, util::chrono::microseconds>;

//
// Utility functions
//

namespace detail
{
    inline Timepoint& baseTime() _ut_noexcept
    {
        static Timepoint sBaseTime;
        return sBaseTime;
    }
}

inline void rebaseMonotonicTime() _ut_noexcept
{
    detail::baseTime() = util::chrono::time_point_cast<
        Timepoint::duration>(Timepoint::clock::now());
}

inline Timepoint monotonicTime() _ut_noexcept
{
    return Timepoint(util::chrono::duration_cast<
        Timepoint::duration>(Timepoint::clock::now() - detail::baseTime()));
}

inline int64_t monotonicMicroseconds() _ut_noexcept
{
    return monotonicTime().time_since_epoch().count();
}

inline long monotonicMilliseconds() _ut_noexcept
{
    return (long) (monotonicMicroseconds() / 1000LL);
}

}
