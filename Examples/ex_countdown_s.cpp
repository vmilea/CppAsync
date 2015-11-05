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

#ifdef HAVE_BOOST_CONTEXT

#include "Common.h"
#include "util/IO.h"
#include "util/Looper.h"
#include <CppAsync/StackfulAsync.h>

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

void ex_countdown_s()
{
    // 5 second countdown
    const int n = 5;

    // startAsync() packages an asynchronous coroutine as a Task. Tasks are a generic, composable
    // representation of asynchronous operations. They serve as building blocks that may be awaited
    // from within other asynchronous coroutines.
    ut::Task<void> task = ut::stackful::startAsync([n]() {
        for (int i = n; i > 0; i--) {
            printf("%d\n", i);

            // Suspend for 1 second.
            ut::stackful::await_(asyncDelay(1000));
        }

        printf("liftoff!\n");

        // Stop pinging when done.
        sLooper.cancelAll();
    });

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

#endif // HAVE_BOOST_CONTEXT
