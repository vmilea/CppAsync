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

// No support for stackful coroutines if exceptions are banned. Wasting
// address space in such restricted environments would be silly anyway.
#ifndef UT_NO_EXCEPTIONS

#include "impl/AwaitableOps.h"
#include "util/Meta.h"
#include "Task.h"

//
// Fwd declarations
//

namespace ut {

namespace stackful
{
    template <class R = void>
    struct Context;
}

}

#include "impl/StackfulAsyncImpl.h"


namespace ut {

namespace stackful
{
    template <class R>
    struct Context
    {
        Promise<R> takePromise() _ut_noexcept
        {
            using stash_type = detail::stackful::AsyncCoroutineAwaiter<R>;

            auto& promise = static_cast<stash_type&>( // safe cast
                detail::stackful::context::currentStash()).promise();

            ut_dcheck(promise.state() != PromiseBase::ST_Moved &&
                "Promise already taken");

            ut_dcheck(promise.isCompletable() &&
                "Can't take promise after detaching task");

            return std::move(promise);
        }
    };

    //
    // Instance generators
    //

    template <class F, class StackAllocator,
        EnableIf<IsFunctor<Unqualified<F>>::value> = nullptr>
    auto startAsync(F&& f, BasicStackAllocator<StackAllocator> stackAllocator)
        -> Task<typename detail::stackful::AsyncFunctionTraits<F>::result_type>
    {
        using result_type = typename detail::stackful::AsyncFunctionTraits<F>::result_type;
        using function_type = detail::stackful::AsyncCoroutineFunction<F>;
        using awaiter_type = detail::stackful::AsyncCoroutineAwaiter<result_type>;
        using coroutine_type = detail::stackful::StackfulCoroutine;
        using listener_type = detail::TaskMaster<result_type, coroutine_type>;

        auto task = makeTaskWithListener<listener_type>(coroutine_type(
            TypeInPlaceTag<function_type>(), std::move(f), stackAllocator));
        auto& coroutine = task.template listenerAs<listener_type>().resource;
        auto& awaiter = *static_cast<awaiter_type*>(coroutine.raw().functionPtr()); // safe cast
        awaiter.start(&coroutine.raw(), task.takePromise());

        return task;
    }

    template <class StackAllocator = FixedSizeStack, class F,
        EnableIf<IsFunctor<Unqualified<F>>::value> = nullptr>
    auto startAsync(F&& f, int stackSize = StackAllocator::traits_type::default_size())
        -> Task<typename detail::stackful::AsyncFunctionTraits<F>::result_type>
    {
        return startAsync(std::forward<F>(f), StackAllocator(stackSize));
    }

    template <class T, class R, class StackAllocator>
    Task<R> startAsync(T *object, R (T::*method)(),
        BasicStackAllocator<StackAllocator> stackAllocator)
    {
        return startAsync([object, method]() -> R {
            return (object->*method)();
        }, std::move(stackAllocator));
    }

    template <class StackAllocator = FixedSizeStack, class T, class R>
    Task<R> startAsync(T *object, R (T::*method)(),
        int stackSize = StackAllocator::traits_type::default_size())
    {
        return startAsync(object, method, StackAllocator(stackSize));
    }

    //
    // Stackful await
    //

    template <class ...Awaitables>
    inline AwaitableBase* awaitAnyNoThrow_(Awaitables&&... awts)
    {
        static_assert(All<detail::IsPlainOrRvalueAwtBaseReference<Awaitables&&>...>::value,
            "stackful::awaitAny_() expects awts to be rvalue or non-const lvalue references "
            "to AwaitableBase");

        AwaitableBase* list[] = { &awts... };

        // May throw throw only ut::ForcedUnwind.
        return detail::stackful::rAwaitAnyNoThrow_(makeRange(list));
    }

    template <class ...Awaitables>
    inline AwaitableBase* awaitAny_(Awaitables&&... awts)
    {
        static_assert(All<detail::IsPlainOrRvalueAwtBaseReference<Awaitables&&>...>::value,
            "stackful::awaitAny_() expects awts to be rvalue or non-const lvalue references "
            "to AwaitableBase");

        AwaitableBase* list[] = { &awts... };
        return detail::stackful::rAwaitAny_(makeRange(list));
    }

    template <class ...Awaitables>
    inline AwaitableBase* awaitAllNoThrow_(Awaitables&&... awts)
    {
        static_assert(All<detail::IsPlainOrRvalueAwtBaseReference<Awaitables&&>...>::value,
            "stackful::awaitAll_() expects awts to be rvalue or non-const lvalue references "
            "to AwaitableBase");

        AwaitableBase* list[] = { &awts... };

        // May throw throw only ut::ForcedUnwind.
        return detail::stackful::rAwaitAllNoThrow_(makeRange(list));
    }

    template <class ...Awaitables>
    inline void awaitAll_(Awaitables&&... awts)
    {
        static_assert(All<detail::IsPlainOrRvalueAwtBaseReference<Awaitables&&>...>::value,
            "stackful::awaitAll_() expects awts to be rvalue or non-const lvalue references "
            "to AwaitableBase");

        AwaitableBase* list[] = { &awts... };
        detail::stackful::rAwaitAll_(makeRange(list));
    }

    template <class Awaitable>
    void awaitNoThrow_(Awaitable&& awt)
    {
        static_assert(IsPlainOrRvalueReference<Awaitable&&>::value,
            "stackful::await_() expects awt to be an rvalue or non-const lvalue reference "
            "supported via AwaitableTraits<T>");

        // May throw throw only ut::ForcedUnwind.
        detail::stackful::awaitImpl_(awt);
    }

    template <class Awaitable,
        EnableIfVoid<AwaitableResult<Unqualified<Awaitable>>> = nullptr>
    void await_(Awaitable&& awt)
    {
        awaitNoThrow_(std::forward<Awaitable>(awt));

        if (detail::awaitable::hasError(awt))
            rethrowException(detail::awaitable::takeError(awt));
    }

    template <class Awaitable,
        DisableIfVoid<AwaitableResult<Unqualified<Awaitable>>> = nullptr>
    AwaitableResult<Unqualified<Awaitable>> await_(Awaitable&& awt)
    {
        awaitNoThrow_(std::forward<Awaitable>(awt));

        if (detail::awaitable::hasError(awt))
            rethrowException(detail::awaitable::takeError(awt));

        return detail::awaitable::takeResult(awt);
    }
}

}

#endif // UT_NO_EXCEPTIONS
