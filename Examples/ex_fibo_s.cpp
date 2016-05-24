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

#ifdef HAVE_BOOST_CONTEXT

#include "Common.h"
#include "util/IO.h"
#include <CppAsync/StackfulCoroutine.h>
#include <climits>

void ex_fibo_s()
{
    // Generate an "infinite" Fibonacci sequence.
    const int n = INT_MAX;

    // Initialize a stackful coroutine. The lambda function will run on a separate
    // stack, similarly to a kernel thread. However, preemption is not automatic and
    // the coroutine is responsible for yielding back to its caller.
    //
    ut::Coroutine fibo = ut::stackful::makeCoroutine([n]() {
        int a = 0;
        int b = 1;

        for (int i = 0; i < n; i++) {
            // Suspend coroutine.
            ut::stackful::yield_(&b);

            // Coroutine has been resumed, generate next value.
            int t = a;
            a = b;
            b += t;

            if (b < a)
                throw std::runtime_error("overflow");
        }
    });

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
        printf("exception: %s\n", e.what());
    }
}

#endif // HAVE_BOOST_CONTEXT
