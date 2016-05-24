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

#pragma once

#include "impl/Common.h"
#include "Coroutine.h"

//
// Fwd declarations
//

namespace ut {

template <class State>
struct BasicFrame;

template <class Frame>
class StacklessCoroutine;

}

#include "impl/StacklessCoroutineImpl.h"


#define ut_coro_begin_function(coroState) \
    auto& _ut_coroState = coroState; \
    uint32_t _ut_resumePoint = _ut_coroState.resumePoint(); \
    _ut_coroState.setLastLine(0); \
    switch (_ut_resumePoint) { case 0:

#define ut_coro_begin() \
    ut_coro_begin_function(this->coroState())

#define ut_coro_end() \
    return; \
    default: \
        ut_dcheck(false && \
            "Invalid resume point. Please check for mismatched ut_coro_catch."); \
        return; \
    }

#define ut_coro_suspend_() \
    _ut_multi_line_macro_begin \
    _ut_coroState.setLastLine(__LINE__); \
    return; \
    case __LINE__: ; \
    _ut_multi_line_macro_end

#define ut_coro_yield_(x) \
    _ut_multi_line_macro_begin \
    _ut_coroState.lastValue = x; \
    ut_coro_suspend_(); \
    _ut_multi_line_macro_end

#define ut_coro_set_exception_handler(handlerId) \
    _ut_coroState.setExceptionHandler(handlerId)

#define ut_coro_clear_exception_handler() \
    _ut_coroState.clearExceptionHandler()

#define ut_coro_handler(handlerId) \
    case (uint32_t) handlerId << 24: \
    _ut_coroState.clearExceptionHandler(); \
    if (!ut::isNil(ut::detail::stackless::context::loopbackException())) \
        try { \
            auto eptr = std::move(ut::detail::stackless::context::loopbackException()); \
            ut::reset(ut::detail::stackless::context::loopbackException()); \
            ut::rethrowException(std::move(eptr)); \
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

template <class State>
struct BasicFrame
{
    using coro_state_type = State;

    BasicFrame() = default;

    const State& coroState() const _ut_noexcept
    {
        return mState;
    }

    State& coroState() _ut_noexcept
    {
        return mState;
    }

private:
    BasicFrame(const BasicFrame& other) = delete;
    BasicFrame& operator=(const BasicFrame& other) = delete;

    State mState;
};

using CoroState = detail::stackless::CoroStateImpl;

using Frame = BasicFrame<CoroState>;

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
        return mFrame.coroState().isDone();
    }

    void* value() const _ut_noexcept
    {
        return mFrame.coroState().lastValue;
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

#ifndef UT_NO_EXCEPTIONS
        if (status.value == StacklessCoroutineStatus::SC_Done) {
            auto& loopbackException = detail::stackless::context::loopbackException();

            if (!isNil(loopbackException)) {
                auto eptr = std::move(loopbackException);
                reset(loopbackException);
                rethrowException(std::move(eptr));
            }
        }
#endif

        return status;
    }

    StacklessCoroutineStatus resume(void *arg = nullptr) _ut_noexcept
    {
        ut_dcheck(!isDone() &&
            "Can't resume a coroutine after it has finished");

        auto& loopbackException = detail::stackless::context::loopbackException();
        (void) loopbackException;

        auto& coroState = mFrame.coroState();
        coroState.lastValue = nullptr;

        bool isDestructed = false;
        ut_assert(mDestructGuard == nullptr);
        mDestructGuard = &isDestructed;

        bool loopback = false;
        do {
            ut_assert(coroState.lastValue == nullptr);
            ut_assert(isNil(loopbackException)
                || (loopback && (coroState.exceptionHandler() != 0)));

#ifdef UT_NO_EXCEPTIONS
            // Resume from last line or from exception handler.
            detail::stackless::CoroutineFrameTraits<frame_type>::call(mFrame, arg);
#else
            loopback = false;
            try {
                // Resume from last line or from exception handler.
                detail::stackless::CoroutineFrameTraits<frame_type>::call(mFrame, arg);

                ut_assert(isNil(loopbackException));
            } catch (...) {
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
#endif
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

template <class CustomFrame, class Alloc, class ...Args>
Coroutine makeCoroutineOf(std::allocator_arg_t, const Alloc& alloc, Args&&... frameArgs)
{
    detail::stackless::CoroutineAllocHandle<CustomFrame, Alloc> handle(alloc,
        std::forward<Args>(frameArgs)...);

#ifdef UT_NO_EXCEPTIONS
        if (handle == nullptr)
            return Coroutine(); // Return invalid coroutine.
#endif

    return Coroutine::wrap(std::move(handle));
}

template <class CustomFrame, class Arg0, class ...Args,
    EnableIf<!IsAllocatorArg<Arg0>::value> = nullptr>
Coroutine makeCoroutineOf(Arg0&& frameArg0, Args&&... frameArgs)
{
    return makeCoroutineOf<CustomFrame>(std::allocator_arg, std::allocator<char>(),
        std::forward<Arg0>(frameArg0), std::forward<Args>(frameArgs)...);
}

template <class CustomFrame>
Coroutine makeCoroutineOf()
{
    return makeCoroutineOf<CustomFrame>(std::allocator_arg, std::allocator<char>());
}

template <class Alloc = std::allocator<char>, class F,
    EnableIf<IsFunctor<Unqualified<F>>::value> = nullptr>
Coroutine makeCoroutine(F&& f, const Alloc& alloc = Alloc())
{
    static_assert(detail::stackless::CoroutineFunctionTraits<F>::valid, "");

    struct Frame : ut::Frame
    {
        Frame(F&& f) : mF(std::move(f)) { }

        void operator()() { mF(this->coroState()); }

    private:
        F mF;
    };

    return makeCoroutineOf<Frame>(std::allocator_arg, alloc, std::move(f));
}

template <class Alloc = std::allocator<char>, class T>
Coroutine makeCoroutine(T *object, void (T::*method)(CoroState&), const Alloc& alloc = Alloc())
{
    return makeCoroutine([object, method](CoroState& coroState) {
        (object->*method)(coroState);
    }, alloc);
}

}
