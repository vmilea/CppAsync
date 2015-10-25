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
#ifndef UT_DISABLE_EXCEPTIONS

#include "Coroutine.h"
#include <exception>

//
// Fwd declarations
//

namespace ut {

    namespace stackful
    {
        class ForcedUnwind
        {
        public:
            static Error ptr() _ut_noexcept
            {
                static auto sEptr = makeExceptionPtr(ForcedUnwind());
                return sEptr;
            }
        };
    }
}

#include "impl/StackfulCoroutineImpl.h"

namespace ut {

namespace stackful
{
    //
    // StackfulCoroutine
    //

    class StackfulCoroutine
    {
    public:
        StackfulCoroutine() _ut_noexcept
            : mCoroutine(nullptr) { }

        template <class F, class U, class StackAllocator>
        StackfulCoroutine(TypeInPlaceTag<F>, U&& f, StackAllocator stackAllocator)
        {
            static_assert(detail::stackful::CoroutineFunctionTraits<F>::valid, "");

            using impl_type = detail::stackful::CoroutineImpl<F, StackAllocator>;

            auto stackContext = stackAllocator.allocate();

            // Round up to a multiple of max alignment.
            static const size_t implSize = RoundUp<sizeof(impl_type), max_align_size>::value;

            char *sp = static_cast<char*>(stackContext.sp) - implSize; // safe cast
            size_t size = stackContext.size - implSize;

            // Emplace header at the beginning of stack.
            mCoroutine = new (sp) impl_type(
                std::forward<U>(f), std::move(stackAllocator), stackContext, sp, size);
        }

        template <class F, class StackAllocator>
        StackfulCoroutine(F&& f, StackAllocator stackAllocator)
            : StackfulCoroutine(
                TypeInPlaceTag<Unqualified<F>>(), std::forward<F>(f),
                stackAllocator) { }

        StackfulCoroutine(StackfulCoroutine&& other) _ut_noexcept
            : mCoroutine(ut::movePtr(other.mCoroutine)) { }

        StackfulCoroutine& operator=(StackfulCoroutine&& other) _ut_noexcept
        {
            ut_assert(this != &other);

            if (mCoroutine != nullptr)
                mCoroutine->deallocate();

            mCoroutine = ut::movePtr(other.mCoroutine);

            return *this;
        }

        ~StackfulCoroutine() _ut_noexcept
        {
            if (mCoroutine != nullptr)
                mCoroutine->deallocate();
        }

        bool isValid() const _ut_noexcept
        {
            return mCoroutine != nullptr;
        }

        bool isDone() const _ut_noexcept
        {
            ut_dcheck(isValid());

            return mCoroutine->isDone();
        }

        void* value() const _ut_noexcept
        {
            ut_dcheck(isValid());

            return mCoroutine->value();
        }

        template <class T>
        T& valueAs() const _ut_noexcept
        {
            return *static_cast<T*>(value()); // safe cast if T is original type
        }

        bool operator()(void *arg = nullptr)
        {
            ut_dcheck(isValid());

            return (*mCoroutine)(arg);
        }

        detail::stackful::CoroutineImplBase& raw() _ut_noexcept
        {
            ut_dcheck(isValid());

            return *mCoroutine;
        }

        detail::stackful::CoroutineImplBase* release() _ut_noexcept
        {
            return movePtr(mCoroutine);
        }

    private:
        StackfulCoroutine(const StackfulCoroutine& other) = delete;
        StackfulCoroutine& operator=(const StackfulCoroutine& other) = delete;

        detail::stackful::CoroutineImplBase *mCoroutine;
    };

    //
    // Traits
    //

    struct StackTraits
    {
        static size_t minimumSize() _ut_noexcept
        {
            return boost::context::stack_traits::minimum_size();
        }

        static size_t defaultSize() _ut_noexcept
        {
            return boost::context::stack_traits::default_size();
        }
    };

    //
    // Stack allocators
    //

    template <class T>
    class BasicStackAllocator
    {
    public:
        using traits_type = typename T::traits_type;

        BasicStackAllocator(size_t size = traits_type::default_size()) _ut_noexcept
            : mCore(size) { }

        boost::context::stack_context allocate()
        {
            return mCore.allocate();
        }

        void deallocate(boost::context::stack_context& sctx) _ut_noexcept
        {
            mCore.deallocate(sctx);
        }

    private:
        T mCore;
    };

    using FixedSizeStack = BasicStackAllocator<
        boost::context::fixedsize_stack>;

    using ProtectedFixedSizeStack = BasicStackAllocator<
        boost::context::protected_fixedsize_stack>;

#if defined(BOOST_USE_SEGMENTED_STACKS) && !defined(BOOST_WINDOWS)
    using SegmentedStack = BasicStackAllocator<
        boost::context::segmented_stack>;
#endif

    //
    // Instance generators
    //

    template <class F, class StackAllocator,
        EnableIf<IsFunctor<Unqualified<F>>::value> = nullptr>
    Coroutine makeCoroutine(F&& f, BasicStackAllocator<StackAllocator> stackAllocator)
    {
        static_assert(std::is_rvalue_reference<F&&>::value,
            "Stackful makeCoroutine() expects an rvalue to the coroutine function");

        return Coroutine::wrap(StackfulCoroutine(std::move(f), std::move(stackAllocator)));
    }

    template <class StackAllocator = FixedSizeStack, class F,
        EnableIf<IsFunctor<Unqualified<F>>::value> = nullptr>
    Coroutine makeCoroutine(F&& f, int stackSize = StackAllocator::traits_type::default_size())
    {
        return makeCoroutine(std::forward<F>(f), StackAllocator(stackSize));
    }

    template <class T, class StackAllocator>
    Coroutine makeCoroutine(T *object, void (T::*method)(),
        BasicStackAllocator<StackAllocator> stackAllocator)
    {
        return makeCoroutine([object, method]() {
            (object->*method)();
        }, std::move(stackAllocator));
    }

    template <class StackAllocator = FixedSizeStack, class T>
    Coroutine makeCoroutine(T *object, void (T::*method)(),
        int stackSize = StackAllocator::traits_type::default_size())
    {
        return makeCoroutine(object, method, StackAllocator(stackSize));
    }

    //
    // Context switch operator
    //

    inline void* yield_(void *value = nullptr)
    {
        ut_dcheck(detail::stackful::context::callChainSize() > 1 &&
            "Only stackful coroutines may call ut::yield_(). "
            "Stackless coroutines should use macros ut_coro_yield_() / ut_await_() instead.");

        auto& currentCoroutine = *detail::stackful::context::currentCoroutine();
        return currentCoroutine.yield_(value); // suspend
    }
}

}

#endif // UT_DISABLE_EXCEPTIONS
