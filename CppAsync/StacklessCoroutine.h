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

#include "impl/Common.h"
#include "Coroutine.h"

//
// Fwd declarations
//

namespace ut {

struct Frame;

template <class Frame>
class StacklessCoroutine;

}

#include "impl/StacklessCoroutineImpl.h"


#define ut_coro_begin() \
    uint32_t _ut_resumePoint = this->ut_coroState.resumePoint(); \
    this->ut_coroState.setLastLine(0); \
    switch (_ut_resumePoint) { case 0:

#define ut_coro_end() \
    return; \
    default: \
        ut_dcheck(false && \
            "Invalid resume point. Please check for mismatched ut_coro_catch."); \
        return; \
    }

#define ut_coro_suspend_() \
    _ut_multi_line_macro_begin \
    this->ut_coroState.setLastLine(__LINE__); \
    return; \
    case __LINE__: ; \
    _ut_multi_line_macro_end

#define ut_coro_yield_(x) \
    _ut_multi_line_macro_begin \
    this->ut_coroState.lastValue = x; \
    ut_coro_suspend_(); \
    _ut_multi_line_macro_end

#define ut_coro_set_exception_handler(handlerId) \
    this->ut_coroState.setExceptionHandler(handlerId)

#define ut_coro_clear_exception_handler() \
    this->ut_coroState.clearExceptionHandler()

#define ut_coro_handler(handlerId) \
    case (uint32_t) handlerId << 24: \
    this->ut_coroState.clearExceptionHandler(); \
    if (!ut::isNil(ut::detail::stackless::context::loopbackException())) \
        try { \
            auto eptr = std::move(ut::detail::stackless::context::loopbackException()); \
            ut::reset(ut::detail::stackless::context::loopbackException()); \
            ut::rethrowException(eptr); \
        } catch /* (const A& e) {
            ...
        } catch (const B& e) {
            ...
        } */

#define ut_coro_try \
    ut_coro_set_exception_handler(1);

#define ut_coro_catch \
    ut_coro_handler(1)

#define ut_coro_try2 \
    ut_coro_set_exception_handler(2);

#define ut_coro_catch2 \
    ut_coro_handler(2)

#define ut_coro_try3 \
    ut_coro_set_exception_handler(3);

#define ut_coro_catch3 \
    ut_coro_handler(3)

// Call before break/continue from ut_coro_try block, otherwise the exception
// handler may remain active outside of its intended scope.
#define ut_coro_abort_try() \
    ut_coro_clear_exception_handler()

namespace ut {

struct Frame
{
    Frame() = default;

    // Internal state
    detail::stackless::FrameState ut_coroState;

private:
    Frame(const Frame& other) = delete;
    Frame& operator=(const Frame& other) = delete;
};

struct StacklessCoroutineStatus
{
    enum Status
    {
        SC_Suspended,
        SC_Destructed,
        SC_Done
    };

    StacklessCoroutineStatus(Status value) _ut_noexcept
        : value(value) { }

    operator bool() _ut_noexcept
    {
        return value == SC_Suspended;
    }

    Status value;
};

template <class CustomFrame>
class StacklessCoroutine
{
public:
    using frame_type = CustomFrame;

    template <class ...Args>
    StacklessCoroutine(Args&&... frameArgs)
        : mFrame(std::forward<Args>(frameArgs)...)
        , mDestructGuard(nullptr) { }

    ~StacklessCoroutine() _ut_noexcept
    {
        if (mDestructGuard != nullptr)
            *mDestructGuard = true;
    }

    bool isDone() const _ut_noexcept
    {
        return mFrame.ut_coroState.isDone();
    }

    void* value() const _ut_noexcept
    {
        return mFrame.ut_coroState.lastValue;
    }

    const CustomFrame& frame() const _ut_noexcept
    {
        return mFrame;
    }

    CustomFrame& frame() _ut_noexcept
    {
        return mFrame;
    }

    bool operator()(void *arg = nullptr)
    {
        StacklessCoroutineStatus status = resume(arg);

        if (status.value == StacklessCoroutineStatus::SC_Done) {
            auto& loopbackException = detail::stackless::context::loopbackException();

            if (!isNil(loopbackException)) {
                auto eptr = loopbackException;
                reset(loopbackException);
                rethrowException(eptr);
            }
        }

        return status;
    }

    StacklessCoroutineStatus resume(void *arg = nullptr) _ut_noexcept
    {
        ut_dcheck(!isDone() &&
            "Can't resume a coroutine after it has finished");

        auto& loopbackException = detail::stackless::context::loopbackException();
        auto& coroState = mFrame.ut_coroState;

        coroState.lastValue = nullptr;

        bool isDestructed = false;
        ut_assert(mDestructGuard == nullptr);
        mDestructGuard = &isDestructed;

        bool loopback = false;
        do {
            ut_assert(coroState.lastValue == nullptr);
            ut_assert(isNil(loopbackException)
                || (loopback && (coroState.exceptionHandler() != 0)));

            loopback = false;
            _ut_try {
                // Resume from last line or from exception handler.
                detail::stackless::CoroutineFrameTraits<frame_type>::call(mFrame, arg);

                ut_assert(isNil(loopbackException));
            } _ut_catch (...) {
                ut_assert(!isDestructed);
                ut_assert(coroState.lastLine() == 0);
                ut_assert(coroState.lastValue == nullptr);
                ut_assert(isNil(loopbackException));

                loopbackException = currentException();
                ut_assert(!isNil(loopbackException));

                // Resume coroutine to give it a chance at handling the exception
                if (coroState.exceptionHandler() != 0)
                    loopback = true;
            }
        } while (loopback);

        if (isDestructed) {
            ut_assert(isNil(loopbackException));
            return StacklessCoroutineStatus::SC_Destructed;
        } else {
            mDestructGuard = nullptr;
            ut_assert(!isDone());

            if (coroState.lastLine() == 0) {
                coroState.setDone();
                return StacklessCoroutineStatus::SC_Done;
            } else {
                ut_assert(isNil(loopbackException));
                return StacklessCoroutineStatus::SC_Suspended;
            }
        }
    }

private:
    CustomFrame mFrame;
    bool *mDestructGuard;
};

//
// Instance generators
//

template <class CustomFrame, class Allocator, class ...Args>
Coroutine makeCoroutine(std::allocator_arg_t, const Allocator& allocator, Args&&... frameArgs)
{
    detail::stackless::CoroutineAllocHandle<CustomFrame, Allocator> handle(allocator,
        std::forward<Args>(frameArgs)...);

#ifdef UT_DISABLE_EXCEPTIONS
        if (handle.isNil())
            return Coroutine(); // Return nil coroutine.
#endif

    return Coroutine::wrap(std::move(handle));
}

template <class CustomFrame, class Arg0, class ...Args,
    EnableIf<!IsAllocatorArg<Arg0>::value> = nullptr>
Coroutine makeCoroutine(Arg0&& frameArg0, Args&&... frameArgs)
{
    return makeCoroutine<CustomFrame>(std::allocator_arg, std::allocator<char>(),
        std::forward<Arg0>(frameArg0), std::forward<Args>(frameArgs)...);
}

template <class CustomFrame>
Coroutine makeCoroutine()
{
    return makeCoroutine<CustomFrame>(std::allocator_arg, std::allocator<char>());
}

}
