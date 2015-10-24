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
#include <CppAsync/StacklessCoroutine.h>
#include <climits>

namespace {

// Stackless coroutine frame
//
struct FiboFrame : ut::Frame
{
    FiboFrame(int n)
        : n(n) { }

    void operator()()
    {
        // Temporaries may be defined before coroutine body, or in a nested
        // scope - provided the scope doesn't contain a suspension point.
        int t;

        // Coroutine body must be wrapped between begin() ... end() macros.
        ut_coro_begin();

        // Coroutine entry point
        a = 0;
        b = 1;

        for (i = 0; i < n; i++) {
            // Suspend coroutine.
            ut_coro_yield_(&b);

            // Coroutine has been resumed, generate next value.
            t = a;
            a = b;
            b += t;

            if (b < a)
                throw std::runtime_error("overflow");
        }

        ut_coro_end();
    }

private:
    // Data persisted across suspension points:
    const int n;
    int i, a, b;
};

}

void ex_fibo()
{
    // Generate an "infinite" Fibonacci sequence.
    const int n = INT_MAX;

    // Initialize a stackless coroutine. Stackless coroutines persist their state
    // within the frame object, so they don't waste address space like their stackful
    // counterpart.
    ut::Coroutine fibo = ut::makeCoroutineOf<FiboFrame>(n);

    try {
        // Resume coroutine. Possible outcomes:
        // a) fibo() returns true. Coroutine has yielded some value and suspended itself.
        // b) fibo() returns false. Coroutine has finished.
        // c) fibo() propagates an exception. Coroutine has ended in error.
        //
        while (fibo()) {
            // Coroutine has yielded, print value.
            printf("%d\n", fibo.valueAs<int>());
        }
    } catch (const std::exception& e) {
        printf ("exception: %s", e.what());
    }
}
