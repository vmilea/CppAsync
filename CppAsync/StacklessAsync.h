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


#define ut_begin() \
    ut::AwaitableBase *_utAwt; \
    (void) _utAwt; \
    ut_coro_begin()

#define ut_end() \
    ut_coro_end()

#define ut_return(result) \
    _ut_multi_line_macro_begin \
    \
    this->ut_asyncState.promise->complete(result); \
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
    if (ut::detail::stackless::awaitHelper0(*this->ut_asyncState.self, awt)) { \
        this->ut_coroState.setLastLine(__LINE__); \
        return; \
        case __LINE__: \
        ut::detail::stackless::awaitHelper1(this->ut_asyncState.arg, awt); \
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
    if (ut::detail::stackless::awaitAnyHelper0(*this->ut_asyncState.self, outDoneAwt, \
            first, second, ##__VA_ARGS__)) { \
        this->ut_coroState.setLastLine(__LINE__); \
        return; \
        case __LINE__: \
        ut::detail::stackless::awaitAnyHelper1(this->ut_asyncState.resumer(), outDoneAwt, \
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
    if (ut::detail::stackless::awaitAll_Helper0(*this->ut_asyncState.self, outFailedAwt, \
            first, second, ##__VA_ARGS__)) { \
        this->ut_coroState.setLastLine(__LINE__); \
        return; \
        case __LINE__: \
        if (ut::detail::stackless::awaitAll_Helper1(this->ut_asyncState.resumer(), outFailedAwt, \
                first, second, ##__VA_ARGS__)) { \
            this->ut_coroState.setLastLine(__LINE__); \
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
struct AsyncFrame : ut::Frame
{
    using result_type = R;

    AsyncFrame(const void *startupArg = nullptr)
        : ut_asyncState(const_cast<void*>(startupArg)) { }

    Promise<R> takePromise() _ut_noexcept
    {
        Promise<R>& promise = *ut_asyncState.promise;

        ut_dcheck(promise.state() != PromiseBase::ST_Moved &&
            "Promise already taken");

        ut_dcheck(promise.isCompletable() &&
            "Can't take promise after detaching task");

        return std::move(promise);
    }

    void* arg() const
    {
        return ut_asyncState.arg;
    }

    template <class T>
    T& argAs() const
    {
        return *static_cast<T*>(arg()); // safe cast if T is original type
    }

    // Internal state
    detail::stackless::AsyncFrameState<R> ut_asyncState;
};

template <class CustomFrame, class Allocator, class ...Args>
auto startAsync(std::allocator_arg_t, const Allocator& allocator, Args&&... frameArgs)
    -> Task<typename detail::stackless::AsyncFrameTraits<CustomFrame>::result_type>
{
    using result_type = typename detail::stackless::AsyncFrameTraits<CustomFrame>::result_type;
    using awaiter_type = detail::stackless::AsyncCoroutineAwaiter<CustomFrame, Allocator>;
    using awaiter_handle_type = AllocElementPtr<awaiter_type, Allocator>;
    using listener_type = detail::TaskMaster<result_type, awaiter_handle_type>;

    awaiter_handle_type handle(allocator, std::forward<Args>(frameArgs)...);

#ifdef UT_DISABLE_EXCEPTIONS
    if (handle.isNil())
        return Task<result_type>(); // return nil task
#endif

    auto task = makeTaskWithListener<listener_type>(std::move(handle));
    awaiter_type& awaiter = *task.template listenerAs<listener_type>().resource;

    awaiter.coroutine.frame().ut_asyncState.self = &awaiter;
    awaiter.coroutine.frame().ut_asyncState.promise.initialize(task.takePromise());
    awaiter.start();

    return task;
}

template <class CustomFrame, class Arg0, class ...Args,
    EnableIf<!IsAllocatorArg<Arg0>::value> = nullptr>
auto startAsync(Arg0&& frameArg0, Args&&... frameArgs)
    -> Task<typename detail::stackless::AsyncFrameTraits<CustomFrame>::result_type>
{
    return startAsync<CustomFrame>(std::allocator_arg, std::allocator<char>(),
        std::forward<Arg0>(frameArg0), std::forward<Args>(frameArgs)...);
}

template <class CustomFrame>
auto startAsync()
    -> Task<typename detail::stackless::AsyncFrameTraits<CustomFrame>::result_type>
{
    return startAsync<CustomFrame>(std::allocator_arg, std::allocator<char>());
}

}
