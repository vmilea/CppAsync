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

#if defined(_MSC_VER) && _MSC_FULL_VER >= 190024120

#include "Common.h"
#include "util/IO.h"
#include "util/Looper.h"
#include <CppAsync/experimental/TaskCoroutineTraits.h>
#include <string>

namespace {

// Custom run loop
static util::Looper sLooper;

static void ping()
{
    printf(".");
    sLooper.schedule(&ping, 100);
}

static ut::Task<void> asyncDelay(long milliseconds)
{
    ut::Task<void> task;

    // Finish task after delay.
    sLooper.schedule(task.takePromise(), milliseconds);

    return task;
}

static ut::Task<void> asyncDelayedFail(long milliseconds)
{
    ut::Task<void> task;

    // Fail task after delay.
    sLooper.schedule([pr = task.takePromise()]() mutable {
        pr.fail(std::runtime_error("asdasd"));
    }, milliseconds);

    return task;
}

static ut::Task<void> asyncCountdown(int n)
{
    for (int i = n; i > 0; i--) {
        printf("%d\n", i);

        // Suspend for 1 second.
        co_await asyncDelay(1000);
    }

    printf("liftoff!\n");

    // Stop pinging when done.
    sLooper.cancelAll();
}

}

void ex_countdown_n4402()
{
    // 5 second countdown
    const int n = 5;

    // Create an async task on top of stackless 'resumable functions', which are a proposed
    // addition to C++17 or later. See: ISOCPP N4402 and P0057r03.
    ut::Task<void> task = asyncCountdown(n);

    // Print every 100ms to show the even loop is not blocked.
    ping();

    // In order to do meaningful work CppAsync requires some kind of run loop (Qt / GTK /
    // MFC / Boost.Asio / ...). Events should be dispatched to the run loop, enabling
    // coordination of concurrent tasks from this single thread.
    //
    // Here a custom Looper runs until there are no more scheduled operations.
    sLooper.run();

    assert(task.isReady());
}

#endif
