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

#ifdef HAVE_BOOST

#include "Common.h"
#include "util/Looper.h"
#include <CppAsync/Awaitable.h>
#include <boost/thread/future.hpp>

namespace context
{
    inline util::Looper& looper()
    {
        // Custom run loop
        static util::Looper sLooper;
        return sLooper;
    }
}

namespace ut {

//
// Teach library about boost::future<R>. This can be done by specializing either
// AwaitableTraits, or just the relevant shims (awaitable_isReady() & friends).
//

template <class R>
struct AwaitableTraits<boost::future<R>>
{
    static bool isReady(const boost::future<R>& fut) _ut_noexcept
    {
        return !fut.valid() || fut.is_ready();
    }

    static bool hasError(const boost::future<R>& fut) _ut_noexcept
    {
        return fut.has_exception();
    }

    static void setAwaiter(boost::future<R>& fut, ut::Awaiter *awaiter) _ut_noexcept
    {
        fut.then([&fut, awaiter](boost::future<R> previous) {
            // Store result.
            fut = std::move(previous);

            // Make sure resumal happens on main thread.
            context::looper().post([awaiter]() {
                // Awaiter context (including fut object) must not have been destroyed
                // before resumal! For more complex use cases where cancellation needs to be
                // supported, use Task<R> and Promise<R>.
                awaiter->resume(nullptr);
            });
        });
    }

    static R takeResult(boost::future<R>& fut)
    {
        return std::move(fut.get());
    }

    static std::exception_ptr takeError(boost::future<R>& fut) _ut_noexcept
    {
        try {
            boost::rethrow_exception(fut.get_exception_ptr());
            assert(false);
            return std::exception_ptr();
        } catch (...) {
            return std::current_exception();
        }
    }
};

}

#endif // HAVE_BOOST
