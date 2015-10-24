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

#pragma once

#include "Common.h"
#include "Assert.h"
#include "../util/AllocElementPtr.h"
#include "../util/FunctionTraits.h"
#include "../util/Meta.h"
#include <exception>

namespace ut {

namespace detail
{
    namespace stackless
    {
        static const uint32_t CORO_LINE_MASK = 0x00FFFFFF;

        namespace context
        {
            inline Error& loopbackException() _ut_noexcept
            {
                static Error sEptr;
                return sEptr;
            }
        }

        //
        // Internal coroutine state
        //

        struct CoroStateImpl
        {
            void *lastValue;
            uint32_t lastState;

            CoroStateImpl() _ut_noexcept
                : lastValue(nullptr)
                , lastState(0) { }

            bool isDone() const _ut_noexcept
            {
                return lastLine() == CORO_LINE_MASK;
            }

            void setDone() _ut_noexcept
            {
                setLastLine(CORO_LINE_MASK);
            }

            uint32_t lastLine() const _ut_noexcept
            {
                return lastState & CORO_LINE_MASK;
            }

            void setLastLine(uint32_t value) _ut_noexcept
            {
                ut_assert(value <= CORO_LINE_MASK);

                lastState = (lastState & ~CORO_LINE_MASK) | value;
            }

            uint8_t exceptionHandler() const _ut_noexcept
            {
                return (uint8_t) (lastState >> 24);
            }

            void setExceptionHandler(uint8_t id) _ut_noexcept
            {
                ut_dcheck(exceptionHandler() == 0 &&
                    "Multiple level try-catch blocks not supported");
                ut_dcheck(id > 0 &&
                    "Supported handler ID range is 1..255");

                lastState = (lastState & CORO_LINE_MASK) | (id << 24);
            }

            void clearExceptionHandler() _ut_noexcept
            {
                ut_assert(exceptionHandler() != 0);

                lastState &= CORO_LINE_MASK;
            }

            uint32_t resumePoint() const _ut_noexcept
            {
                if (exceptionHandler() != 0 && !isNil(context::loopbackException()))
                    return (uint32_t) exceptionHandler() << 24;
                else
                    return lastLine();
            }
        };

        template <typename T>
        class HasCoroStateType
        {
            template <class U> static std::true_type test(typename U::coro_state_type*);
            template <class U> static std::false_type test(...);

        public:
            using type = decltype(test<T>(nullptr));
            static const bool value = type::value;
        };

        //
        // Frame validation -- for ut::makeCoroutineOf<Frame>()
        //

        template <class CustomFrame>
        struct CoroutineFrameTraits
        {
            static_assert(HasCoroStateType<CustomFrame>::value,
                "Frame must derive from ut::Frame or ut::AsyncFrame<R>");

            static_assert(std::is_base_of<
                    CoroStateImpl, typename CustomFrame::coro_state_type>::value,
                "Frame must derive from ut::Frame or ut::AsyncFrame<R>");

            static_assert(IsFunctor<CustomFrame>::value,
                "Frame must be a functor with signature: "
                "void operator()() or void operator()(void* arg)");

            static_assert(FunctionHasArity<0, 1>::template Type<CustomFrame>::value,
                "Frame must be a functor with signature: "
                "void operator()() or void operator()(void* arg)");

            template <class T, EnableIf<FunctionIsUnary<T>::value> = nullptr>
            static void call(T& fr, void *arg)
            {
                static_assert(std::is_same<FunctionArg<T, 0>, void*>::value,
                    "Frame must be a functor with signature: "
                    "void operator()() or void operator()(void* arg)");

                fr(arg);
            }

            template <class T, EnableIf<FunctionIsNullary<T>::value> = nullptr>
            static void call(T& fr, void * /*ignored */)
            {
                fr();
            }
        };

        //
        // Function validation -- for ut::makeCoroutine(lambda)
        //

        template <class F>
        struct CoroutineFunctionTraits
        {
            static_assert(std::is_rvalue_reference<F&&>::value,
                "Expecting an an rvalue to the coroutine function");

            static_assert(IsFunctor<F>::value,
                "Expected signature of stackless coroutine function: void f(ut::CoroState&)");

            static_assert(FunctionIsUnary<F>::value,
                "Expected signature of stackless coroutine function: void f(ut::CoroState&)");

            static_assert(std::is_same<CoroStateImpl&, FunctionArg<F, 0>>::value,
                "Expected signature of stackless coroutine function: void f(ut::CoroState&)");

            static const bool valid = true;
        };

        //
        // Coroutine handle
        //

        template <class Derived, class T>
        struct CoroutineAllocMixin : AllocElementMixin<Derived, T>
        {
            bool operator()(void *arg)
            {
                return this->core()(arg);
            };

            bool isDone() const _ut_noexcept
            {
                return this->core().isDone();
            }

            void* value() const _ut_noexcept
            {
                return this->core().value();
            }
        };

        template <class CustomFrame, class Alloc>
        using CoroutineAllocHandle = ExtendedAllocElementPtr<
            StacklessCoroutine<CustomFrame>, Alloc, CoroutineAllocMixin>;
    }
}

}
