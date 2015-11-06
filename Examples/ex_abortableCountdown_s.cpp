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
#include <thread>

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

static ut::Task<std::string> asyncReadLine()
{
    ut::Task<std::string> task;
    auto promise = task.takePromise().share();

    // Read input on a separate thread.
    std::thread([promise]() {
        std::string line = util::readLine();

        // Finish the task on the main thread.
        std::function<void ()> f = [promise, line]() { promise(line); };
        sLooper.schedule(std::move(f));
    }).detach();

    return task;
}

}

void ex_abortableCountdown_s()
{
    // 5 second countdown
    const int n = 5;

    ut::Task<void> task = ut::stackful::startAsync([n]() {
        auto readLineTask = asyncReadLine();

        for (int i = n; i > 0; i--) {
            printf("%d\n", i);

            // Suspend for up to 1 second, or until key press.
            auto *doneTask = ut::stackful::awaitAny_(
                readLineTask, asyncDelay(1000));

            if (doneTask == &readLineTask) {
                printf("aborted!\n");
                sLooper.cancelAll();
                return;
            }
        }

        printf("liftoff!\n");

        // Stop pinging when done.
        sLooper.cancelAll();
    });

    // Print every 100ms to show the even loop is not blocked.
    ping();

    // Loop until there are no more scheduled operations.
    sLooper.run();

    assert(task.isReady());
}

#endif // HAVE_BOOST_CONTEXT
