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

#ifdef HAVE_BOOST

#include "Common.h"
#include "ex_customAwaitable.h"
#include <CppAsync/Scheduler.h>
#include <CppAsync/StacklessAsync.h>
#include <boost/thread/thread.hpp>

static void ping()
{
    printf(".");
    context::looper().schedule(&ping, 100);
}

static boost::shared_future<int> startTick(int k)
{
    boost::shared_future<int> future;

    if (k > 0) {
        boost::packaged_task<int> pt([k]() {
            boost::thread::sleep(
                boost::get_system_time() + boost::posix_time::millisec(500));
            return k;
        });
        future = pt.get_future().share();
        boost::thread(std::move(pt)).detach();
    } else {
        future = startTick(1).then([](boost::shared_future<int> previous) {
            throw std::runtime_error("blow up!");
            return 0;
        });
    }

    return future;
}

static ut::Task<void> asyncCountdown()
{
    struct Frame : ut::AsyncFrame<void>
    {
        ~Frame()
        {
            // Stop pinging when done.
            context::looper().cancelAll();
        }

        void operator()()
        {
            ut_begin();

            for (i = 3; i >= 0; i--) {
                future = startTick(i);

                // Await future directly -- without wrapping in a ut::Task.
                ut_await_(future);

                // Combinators are not allowed unless awaited type is derived from
                // ut::AwaitableBase (usually via ut::CommonAwaitable).
                // ut_await_all_(future, future); // Won't compile!

                printf("tick %d\n", future.get());
            }
            ut_end();
        }

    private:
        int i;
        boost::shared_future<int> future;
    };

    return ut::startAsyncOf<Frame>();
}

void ex_customAwaitable()
{
    ut::Task<void> task = asyncCountdown();

    // Print every 100ms to show the even loop is not blocked.
    ping();

    // Loop until there are no more scheduled operations.
    context::looper().run();

    assert(task.isReady());
    try {
        task.get();
    } catch (const std::exception& e) {
        printf("exception: %s\n", e.what());
    }
}

#endif // HAVE_BOOST
