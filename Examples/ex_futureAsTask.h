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
#include <CppAsync/Task.h>
#include <CppAsync/util/MoveOnCopy.h>
#include <boost/thread/future.hpp>
#include <functional>

namespace context
{
    inline util::Looper& looper()
    {
        // Custom run loop
        static util::Looper sLooper;
        return sLooper;
    }
}

namespace detail
{
    template <class R>
    void complete(ut::Promise<R>&& promise, boost::future<R>& future) _ut_noexcept
    {
        std::exception_ptr eptr;
        try {
            R&& result = future.get();
            promise.complete(std::move(result));
            return;
        } catch (...) {
            eptr = std::current_exception();
        }
        promise.fail(eptr);
    }

    inline void complete(ut::Promise<void>&& promise, boost::future<void>& future) _ut_noexcept
    {
        std::exception_ptr eptr;
        try {
            future.get();
            promise.complete();
            return;
        } catch (...) {
            eptr = std::current_exception();
        }
        promise.fail(eptr);
    }
}

template <class R>
inline ut::Task<R> futureAsTask(boost::future<R> future)
{
    ut::Task<R> task;

    if (future.is_ready()) {
        detail::complete(task.takePromise(), future);
    } else {
        auto promise = task.takePromise().share();
        auto next = std::make_shared<boost::future<void>>();

        // Complete Task after continuation. Due to a quirk in Boost's future API we need to keep
        // the `next` future alive until the continuation has run, otherwise it would block in its
        // destructor.
        *next = future.then(boost::launch::async,
                [next, promise](boost::future<R> previous) {
            assert(previous.is_ready());

            // Workaround for lack of move-capture in MSVC 12.0
            auto mvPrevious = ut::makeMoveOnCopy(std::move(previous));

            // Make sure completion happens on main thread.
            std::function<void ()> f = [mvPrevious, next, promise]() {
                // std::weak_ptr<boost::future<void> weakNext(next); // COMPILER STALL

                // Break circular references.
                *next = boost::future<void>();
                assert(next.use_count() == 1);

                // Nothing to do if Task got canceled in the meantime.
                if (promise.isCompletable())
                    detail::complete(std::move(promise.promise()), *mvPrevious);
            };
            context::looper().post(f);
        });
    }

    return task;
}

#endif // HAVE_BOOST
