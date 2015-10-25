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
#include "../Examples/util/Looper.h"
#include <CppAsync/StacklessAsync.h>
#include <CppAsync/util/Arena.h>

namespace {

// Custom run loop
static util::Looper sLooper;

static ut::Task<void> asyncDelay(long milliseconds)
{
    auto task = ut::makeTask();
    auto promise = task.takePromise().share();

    // Finish task after delay. Assuming scheduler can't fail.
    sLooper.schedule([promise]() {
        promise.complete();
    }, milliseconds);

    return task;
}

enum ErrorId
{
    BlownUpError = -1
};

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

        printf("blow up!\n");
        ut_return_error(BlownUpError); // Fail task.

        ut_end();
    }

private:
    int n;
    ut::Task<void> subtask;
};

}

void ex_countdown()
{
    // Use a custom allocator. When exceptions are disabled, allocate() may return
    // null to indicate failure.
    ut::LinearStackArena<128> arena;
    auto alloc = ut::makeArenaAlloc(arena);

    // 5 second countdown
    const int n = 5;

    ut::Task<void> task = ut::startAsyncOf<CountdownFrame>(
        std::allocator_arg, alloc, n);

    // If allocation fails, the returned Task will be invalid.
    if (!task.isValid()) {
        printf("error: allocation failed\n");
        return;
    }

    // Loop until there are no more scheduled operations.
    sLooper.run();

    assert(task.isReady());
    if (task.hasError())
        printf("error: %d\n", task.error().get());
}
