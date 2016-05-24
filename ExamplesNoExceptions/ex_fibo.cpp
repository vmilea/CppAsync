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

#include "Common.h"
#include <CppAsync/StacklessCoroutine.h>
#include <CppAsync/util/Arena.h>
#include <climits>

namespace {

enum ErrorId
{
    OverflowError = -1
};

struct FiboFrame : ut::Frame
{
    FiboFrame(int n)
        : n(n) { }

    void operator()()
    {
        int t;
        ut_coro_begin();

        a = 0; b = 1;

        for (i = 0; i < n; i++) {
            ut_coro_yield_(&b);

            t = a; a = b; b += t;

            if (b < a) {
                b = OverflowError;
                ut_coro_yield_(&b);

                return;
            }
        }

        ut_coro_end();
    }

private:
    const int n;
    int i, a, b;
};

}

void ex_fibo()
{
    // Use a custom allocator. When exceptions are disabled, allocate() may return
    // null to indicate failure.
    ut::LinearStackArena<64> arena;
    auto alloc = ut::makeArenaAlloc(arena);

    const int n = INT_MAX;

    ut::Coroutine fibo = ut::makeCoroutineOf<FiboFrame>(
        std::allocator_arg, alloc, n);

    // If allocation fails, the returned Coroutine will be invalid.
    if (!fibo.isValid()) {
        printf("error: allocation failed\n");
        return;
    }

    // Resume coroutine. Possible outcomes:
    // a) fibo() returns true. Coroutine has yielded some value and suspended itself.
    // b) fibo() returns false. Coroutine has finished.
    //
    while (fibo()) {
        int value = fibo.valueAs<int>();

        if (value < 0) {
            printf("error: %d\n", value);
            return;
        }

        // Coroutine has yielded, print value.
        printf("%d\n", value);
    }
}
