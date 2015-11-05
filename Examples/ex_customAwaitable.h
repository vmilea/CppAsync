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

#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION

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
// Teach library about boost::shared_future<R>. This can be done by specializing
// either AwaitableTraits, or just the relevant shims (awaitable_isReady() & friends).
//

template <class R>
struct AwaitableTraits<boost::shared_future<R>>
{
    static bool isReady(const boost::shared_future<R>& fut) _ut_noexcept
    {
        return !fut.valid() || fut.is_ready();
    }

    static bool hasError(const boost::shared_future<R>& fut) _ut_noexcept
    {
        return fut.has_exception();
    }

    static void setAwaiter(boost::shared_future<R>& fut, ut::Awaiter *awaiter) _ut_noexcept
    {
        // Futures returned by boost::shared_future<R>::then() will block in their destructor
        // until completed. To avoid this we capture the returned future inside continuation.
        auto next = std::make_shared<boost::future<void>>();

        *next = fut.then([next, awaiter](boost::shared_future<R> previous) {
            // Make sure resumal happens on main thread.
            context::looper().post([next, awaiter]() {
                // Break circular references.
                *next = boost::future<void>();

                awaiter->resume(nullptr);
            });
        });
    }

    static R takeResult(boost::shared_future<R>& fut)
    {
        return std::move(fut.get());
    }

    static std::exception_ptr takeError(boost::shared_future<R>& fut) _ut_noexcept
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
