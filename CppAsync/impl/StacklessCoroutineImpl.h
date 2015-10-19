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
        static const uint32_t CORO_END_CATCH_LINE_OFFSET = 10000000;

        namespace context
        {
            inline Error& loopbackException() _ut_noexcept
            {
                static Error sEptr;
                return sEptr;
            }
        }

        struct FrameState
        {
            void *lastValue;
            uint32_t lastState;

            FrameState() _ut_noexcept
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
                return static_cast<uint8_t>((lastState & ~CORO_LINE_MASK) >> 24); // safe cast
            }

            void setExceptionHandler(uint8_t id) _ut_noexcept
            {
                ut_dcheck(id > 0 &&
                    "Supported handler ID range is 1..255");

                lastState = (lastState & CORO_LINE_MASK) | (id << 24);
            }

            void clearExceptionHandler() _ut_noexcept
            {
                lastState &= CORO_LINE_MASK;
            }

            uint32_t resumePoint() const _ut_noexcept
            {
                if (!isNil(context::loopbackException()) && exceptionHandler() != 0)
                    return static_cast<uint32_t>(exceptionHandler()) << 24;
                else
                    return lastLine();
            }
        };

        template <class CustomFrame>
        struct CoroutineFrameTraits
        {
            static_assert(std::is_base_of<ut::Frame, CustomFrame>::value,
                "Frame must derive from ut::Frame");

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

        template <class CustomFrame, class Allocator>
        using CoroutineAllocHandle = ExtendedAllocElementPtr<
            StacklessCoroutine<CustomFrame>, Allocator, CoroutineAllocMixin>;
    }
}

}
