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

namespace {

// Custom run loop
static util::Looper sLooper;

static ut::Task<void> asyncDelay(long milliseconds)
{
    // The delay task is trivial and doesn't need its own coroutine.
    // Instead we manually create a task object, then schedule its
    // completion on the run loop.

    auto task = ut::makeTask();
    auto promise = task.takePromise().share();

    // Finish task after delay.
    sLooper.schedule([promise]() {
        promise.complete();
    }, milliseconds);

    return task;
}

// Stackless asynchronous coroutine frame
//
struct CountdownFrame : ut::AsyncFrame<void>
{
    CountdownFrame(int n)
        : n(n) { }

    void operator()()
    {
        ut_begin();

        while (n > 0) {
            printf("%d...\n", n--);

            // Suspend for 1 second.
            subtask = asyncDelay(1000);
            ut_await_(subtask);
        }

        printf("liftoff!\n");
        ut_end();
    }

private:
    // Data persisted across suspension points:
    int n;
    ut::Task<void> subtask;
};

}

void ex_countdown()
{
    // 5 second countdown
    const int n = 5;

    // startAsync() packages an asynchronous coroutine as a Task. Tasks are a generic, composable
    // representation of asynchronous operations. They serve as building blocks that may be awaited
    // from within other asynchronous coroutines.
    ut::Task<void> task = ut::startAsyncOf<CountdownFrame>(n);

    // In order to do meaningful work CppAsync requires some kind of run loop (Qt / GTK /
    // MFC / Boost.Asio / ...). Events should be dispatched to the run loop, enabling
    // coordination of concurrent tasks from this single thread.
    //
    // Here a custom Looper runs until there are no more scheduled operations.
    sLooper.run();

    assert(task.isReady());
}
