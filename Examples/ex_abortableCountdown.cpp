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

#include "Common.h"
#include "util/IO.h"
#include "util/Looper.h"
#include <CppAsync/StacklessAsync.h>
#include <thread>

namespace {

// Custom run loop
static util::Looper sLooper;

static ut::Task<void> asyncDelay(long milliseconds)
{
    auto task = ut::makeTask();
    auto promise = task.takePromise().share();

    // Finish task after delay.
    sLooper.schedule([promise] {
        promise.complete();
    }, milliseconds);

    return task;
}

static ut::Task<std::string> asyncReadLine()
{
    auto task = ut::makeTask<std::string>();
    auto promise = task.takePromise().share();

    // Read input on a separate thread.
    std::thread([promise]() {
        std::string line = util::readLine();

        // Finish the task on the main thread.
        std::function<void ()> f = [promise, line] {
            promise.complete(line);
        };
        sLooper.schedule(std::move(f));
    }).detach();

    return task;
}

struct CountdownFrame : ut::AsyncFrame<void>
{
    CountdownFrame(int n)
        : n(n) { }

    void operator()()
    {
        ut::AwaitableBase *doneTask;
        ut_begin();

        readLineTask = asyncReadLine();

        while (n > 0) {
            printf("%d...\n", n--);

            delayTask = asyncDelay(1000);

            // Suspend for up to 1 second, or until key press.
            ut_await_any_(doneTask, readLineTask, delayTask);

            if (doneTask == &readLineTask) {
                printf("aborted!\n");
                sLooper.cancelAll();
                return;
            }
        }

        printf("liftoff!\n");
        ut_end();
    }

private:
    int n;
    ut::Task<std::string> readLineTask;
    ut::Task<void> delayTask;
};

}

void ex_abortableCountdown()
{
    // 5 second countdown
    const int n = 5;

    ut::Task<void> task = ut::startAsyncOf<CountdownFrame>(n);

    // Loop until there are no more scheduled operations.
    sLooper.run();

    assert(task.isReady());
}
