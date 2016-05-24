/*
* Copyright 2015-2016 Valentin Milea
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
#include "ex_futureAsTask.h"
#include "util/IO.h"
#include <CppAsync/StackfulAsync.h>
#include <CppAsync/util/ScopeGuard.h>
#include <boost/thread/thread.hpp>

namespace {

static void ping()
{
    printf(".");
    context::looper().schedule(&ping, 100);
}

static boost::future<int> startTick(int k)
{
    boost::future<int> future;

    if (k > 0) {
        boost::packaged_task<int ()> pt([k]() {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
            return k;
        });
        future = pt.get_future();
        boost::thread(std::move(pt)).detach();
    } else {
        future = startTick(1).then([](boost::future<int> previous) {
            throw std::runtime_error("blow up!");
            return 0;
        });
    }

    return future;
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
        context::looper().schedule(std::move(f));
    }).detach();

    return task;
}

}

void ex_futureAsTask_s()
{
    ut::Task<void> task = ut::stackful::startAsync([]() {
        // Stop pinging when done.
        ut_scope_guard_([] { context::looper().cancelAll(); });

        auto readLineTask = asyncReadLine();

        for (int i = 3; i >= 0; i--) {
            auto tickTask = futureAsTask(startTick(i));

            // Suspend until next tick or key press.
            auto *doneTask = ut::stackful::awaitAny_(readLineTask, tickTask);

            if (doneTask == &readLineTask) {
                printf("aborted!\n");

                // Application specific: here the underlying operation responsible for
                // completing the future may be interrupted.

                return;
            }

            printf("tick %d\n", tickTask.get());
        }
    });

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
