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

//
// Fwd declarations
//

namespace ut {

template <class R = void>
struct AsyncFrame;

}

#include "impl/StacklessAsyncImpl.h"


#define ut_begin_function(coroState) \
    ut::AwaitableBase *_utAwt; \
    (void) _utAwt; \
    ut_coro_begin_function(coroState)

#define ut_begin() \
    ut_begin_function(this->coroState())

#define ut_end() \
    ut_coro_end()

#define ut_return(result) \
    _ut_multi_line_macro_begin \
    \
    _ut_coroState.promise.complete(result); \
    return; \
    \
    _ut_multi_line_macro_end

#define ut_return_error(error) \
    _ut_multi_line_macro_begin \
    \
    _ut_coroState.promise.fail(error); \
    return; \
    \
    _ut_multi_line_macro_end

#define ut_try \
    ut_coro_try

#define ut_catch \
    ut_coro_catch

#define ut_try2 \
    ut_coro_try2

#define ut_catch2 \
    ut_coro_catch2

#define ut_try3 \
    ut_coro_try3

#define ut_catch3 \
    ut_coro_catch3

// Call before break/continue from ut_try block, otherwise the exception
// handler may remain active outside of its intended scope.
#define ut_abort_try() \
    ut_coro_abort_try()

#define ut_await_no_throw_(awt) \
    _ut_multi_line_macro_begin \
    \
    if (ut::detail::stackless::awaitHelper0(*_ut_coroState.self, awt)) { \
        _ut_coroState.setLastLine(__LINE__); \
        return; \
        case __LINE__: \
        ut::detail::stackless::awaitHelper1(_ut_coroState.arg, awt); \
        } \
    \
    _ut_multi_line_macro_end

#define ut_await_(awt) \
    _ut_multi_line_macro_begin \
    \
    ut_await_no_throw_(awt); \
    _ut_check_for_error(awt); \
    \
    _ut_multi_line_macro_end

#define ut_await_any_no_throw_(outDoneAwt, first, second, ...) \
    _ut_multi_line_macro_begin \
    \
    if (ut::detail::stackless::awaitAnyHelper0(*_ut_coroState.self, outDoneAwt, \
            first, second, ##__VA_ARGS__)) { \
        _ut_coroState.setLastLine(__LINE__); \
        return; \
        case __LINE__: \
        ut::detail::stackless::awaitAnyHelper1(_ut_coroState.resumer(), outDoneAwt, \
            first, second, ##__VA_ARGS__); \
    } \
    \
    _ut_multi_line_macro_end

#define ut_await_any_(outDoneAwt, first, second, ...) \
    _ut_multi_line_macro_begin \
    \
    ut_await_any_no_throw_(outDoneAwt, first, second, ##__VA_ARGS__); \
    _ut_check_for_error(*outDoneAwt); \
    \
    _ut_multi_line_macro_end

#define ut_await_all_no_throw_(outFailedAwt, first, second, ...) \
    _ut_multi_line_macro_begin \
    \
    if (ut::detail::stackless::awaitAll_Helper0(*_ut_coroState.self, outFailedAwt, \
            first, second, ##__VA_ARGS__)) { \
        _ut_coroState.setLastLine(__LINE__); \
        return; \
        case __LINE__: \
        if (ut::detail::stackless::awaitAll_Helper1(_ut_coroState.resumer(), outFailedAwt, \
                first, second, ##__VA_ARGS__)) { \
            _ut_coroState.setLastLine(__LINE__); \
            return; \
        } \
    } \
    \
    _ut_multi_line_macro_end

#define ut_await_all_(first, second, ...) \
    _ut_multi_line_macro_begin \
    \
    ut_await_all_no_throw_(_utAwt, first, second, ##__VA_ARGS__); \
    if (_utAwt != nullptr) \
        _ut_check_for_error(*_utAwt); \
    \
    _ut_multi_line_macro_end


namespace ut {

template <class R>
using AsyncCoroState = detail::stackless::AsyncCoroStateImpl<R>;

template <class R>
struct AsyncFrame : BasicFrame<AsyncCoroState<R>>
{
    using result_type = R;

    AsyncFrame() = default;

    explicit AsyncFrame(const void *startupArg) _ut_noexcept
    {
        this->coroState().arg = const_cast<void*>(startupArg);
    }

    Promise<R> takePromise() _ut_noexcept
    {
        Promise<R>& promise = this->coroState().promise;

        ut_dcheck(promise.state() != PromiseBase::ST_Empty &&
            "Promise already taken");

        ut_dcheck(promise.isCompletable() &&
            "Can't take promise after detaching or canceling task");

        return std::move(promise);
    }

    void* arg() const _ut_noexcept
    {
        return this->coroState().arg;
    }

    template <class T>
    T& argAs() const _ut_noexcept
    {
        return *static_cast<T*>(arg()); // safe cast if T is original type
    }
};

template <class CustomFrame, class Alloc, class ...Args>
auto startAsyncOf(std::allocator_arg_t, const Alloc& alloc, Args&&... frameArgs)
    -> Task<typename detail::stackless::AsyncFrameTraits<CustomFrame>::result_type>
{
    using result_type = typename detail::stackless::AsyncFrameTraits<CustomFrame>::result_type;
    using awaiter_type = detail::stackless::AsyncCoroutineAwaiter<CustomFrame, Alloc>;
    using awaiter_handle_type = AllocElementPtr<awaiter_type, Alloc>;
    using listener_type = detail::TaskMaster<result_type, awaiter_handle_type>;

    awaiter_handle_type handle(alloc, std::forward<Args>(frameArgs)...);

#ifdef UT_NO_EXCEPTIONS
    if (handle == nullptr) {
        Task<result_type> task;
        task.takePromise();
        return task; // Return invalid task.
    }
#endif

    auto task = makeTaskWithListener<listener_type>(std::move(handle));
    awaiter_type& awaiter = *task.template listenerAs<listener_type>().resource;
    awaiter.coroutine.frame().coroState().self = &awaiter;
    awaiter.coroutine.frame().coroState().promise = task.takePromise();
    awaiter.start();

    return task;
}

template <class CustomFrame, class Arg0, class ...Args,
    EnableIf<!IsAllocatorArg<Arg0>::value> = nullptr>
auto startAsyncOf(Arg0&& frameArg0, Args&&... frameArgs)
    -> Task<typename detail::stackless::AsyncFrameTraits<CustomFrame>::result_type>
{
    return startAsyncOf<CustomFrame>(std::allocator_arg, std::allocator<char>(),
        std::forward<Arg0>(frameArg0), std::forward<Args>(frameArgs)...);
}

template <class CustomFrame>
auto startAsyncOf()
    -> Task<typename detail::stackless::AsyncFrameTraits<CustomFrame>::result_type>
{
    return startAsyncOf<CustomFrame>(std::allocator_arg, std::allocator<char>());
}

template <class Alloc = std::allocator<char>, class F,
    EnableIf<IsFunctor<Unqualified<F>>::value> = nullptr>
auto startAsync(F&& f, const Alloc& alloc = Alloc())
    -> Task<typename detail::stackless::AsyncFunctionTraits<F>::result_type>
{
    using result_type = typename detail::stackless::AsyncFunctionTraits<F>::result_type;

    struct Frame : AsyncFrame<result_type>
    {
        Frame(F&& f) : mF(std::move(f)) { }

        void operator()() { mF(this->coroState()); }

    private:
        F mF;
    };

    return startAsyncOf<Frame>(std::allocator_arg, alloc, std::move(f));
}

template <class Alloc = std::allocator<char>, class T, class R>
Task<R> startAsync(T *object, void (T::*method)(AsyncCoroState<R>&), const Alloc& alloc = Alloc())
{
    return startAsync([object, method](AsyncCoroState<R>& coroState) {
        (object->*method)(coroState);
    }, alloc);
}

}
