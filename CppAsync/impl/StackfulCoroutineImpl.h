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

#ifndef UT_DISABLE_EXCEPTIONS

#include "Assert.h"
#include "../util/Cast.h"
#include "../util/StaticStack.h"
#include "../util/FunctionTraits.h"
#include "../util/Meta.h"
#include "../util/SmartPtr.h"
#include <boost/context/all.hpp>

namespace ut {

namespace detail
{
    namespace stackful
    {
        using namespace ut::stackful;

        class CoroutineImplBase;

        namespace context
        {
            namespace impl
            {
                using call_chain_type = StaticStack<CoroutineImplBase*, UT_MAX_COROUTINE_DEPTH>;

                inline call_chain_type& callChain() _ut_noexcept
                {
                    static call_chain_type sCallChain;
                    return sCallChain;
                }
            }

            inline void initialize();

            inline size_t callChainSize() _ut_noexcept
            {
                return impl::callChain().size();
            }

            inline bool callChainContains(CoroutineImplBase *coroutine) _ut_noexcept
            {
                return impl::callChain().contains(coroutine);
            }

            inline CoroutineImplBase* currentCoroutine() _ut_noexcept
            {
                return impl::callChain().top();
            }

            inline void pushCoroutine(CoroutineImplBase *coroutine) _ut_noexcept
            {
                ut_check(!impl::callChain().isFull() &&
                    "Call chain too deep. Consider increasing UT_MAX_COROUTINE_DEPTH");

                impl::callChain().push(coroutine);
            }

            inline void popCoroutine() _ut_noexcept
            {
                impl::callChain().pop();
            }
        }

        template <class F>
        struct CoroutineFunctionTraits
        {
            static_assert(std::is_rvalue_reference<F&&>::value,
                "Expecting an an rvalue to the coroutine function");

            static_assert(IsFunctor<F>::value,
                "Expected signature of stackful coroutine function: void f() or void f(void *initialValue)");

            static_assert(FunctionHasArityLessThan2<F>::value,
                "Expected signature of stackful coroutine function: void f() or void f(void *initialValue)");

            template <class T, EnableIf<FunctionIsUnary<T>::value> = nullptr>
            static void call(T& f, void *arg)
            {
                static_assert(std::is_same<void *, FunctionArg<F, 0>>::value,
                    "Expected signature of stackful coroutine function: void f() or void f(void *initialValue)");

                f(arg);
            }

            template <class T, EnableIf<FunctionIsNullary<T>::value> = nullptr>
            static void call(T& f, void * /*ignored */)
            {
                f();
            }

            static const bool valid = true;
        };

        class CoroutineImplBase
        {
        public:
            CoroutineImplBase() _ut_noexcept
                : mState(ST_NotStarted)
                , mValue(nullptr)
                , mFContext(boost::context::fcontext_t()) { }

            CoroutineImplBase(void *sp, size_t size) _ut_noexcept
                : CoroutineImplBase()
            {
                mFContext = boost::context::make_fcontext(sp, size, &fcontextFunc);
            }

            virtual ~CoroutineImplBase() _ut_noexcept
            {
                ut_dcheck((context::callChainSize() == 1 || !context::callChainContains(this)) &&
                    "Stackful coroutine may not delete itself while it is executing");

                switch (mState)
                {
                case ST_NotStarted:
                case ST_Done:
                    // Nothing do to.
                    break;
                case ST_Started:
                    forceUnwind();
                    break;
                case ST_Interrupting:
                    ut_assert(false);
                    break;
                default:
                    ut_assert(false);
                    break;
                }
            }

        protected:
            void initialize(void *function) _ut_noexcept
            {
                // Stores a pointer to function for quick acess to stashed data (avoids virtual
                // method). The function object is normally found at (char*)this + sizeof(
                // CoroutineImplBase), but the may be additional padding if alignof(F) > ptr_size.
                mFunction = function;
            }

            virtual void start(void *arg)
            {
                ut_assert(mState == ST_NotStarted);
                mState = ST_Started;
            }

        public:
            virtual void deallocate() _ut_noexcept = 0;

            bool isDone() const _ut_noexcept
            {
                return mState == ST_Done;
            }

            void* value() const _ut_noexcept
            {
                return mValue;
            }

            void* functionPtr() _ut_noexcept
            {
                return mFunction;
            }

            bool operator()(void *arg)
            {
                if (context::callChainSize() == 0)
                    context::initialize();

                auto& parent = *context::currentCoroutine();
                ut_assert(!parent.isDone());

                context::pushCoroutine(this);

                mValue = nullptr;
                mValue = jump(parent, *this, YieldData(YK_Result, arg)); // Suspend.

                return !isDone();
            }

            void* yield_(void *value)
            {
                return yield_(YieldData(YK_Result, value)); // Suspend.
            }

            void* yieldException_(Error *peptr)
            {
                return yield_(YieldData(YK_Exception, peptr)); // Suspend.
            }

        private:
            CoroutineImplBase(const CoroutineImplBase& other) = delete;
            CoroutineImplBase& operator=(const CoroutineImplBase& other) = delete;

            enum State
            {
                ST_NotStarted,
                ST_Started,
                ST_Interrupting,
                ST_Done
            };

            enum YieldKind
            {
                YK_Result,
                YK_Exception
            };

            struct YieldData
            {
                YieldKind kind;
                void *value;

                YieldData(YieldKind kind, void *value) _ut_noexcept
                    : kind(kind)
                    , value(value) { }
            };

            static void fcontextFunc(intptr_t data) _ut_noexcept
            {
                CoroutineImplBase& thiz = *context::currentCoroutine();

                Error *peptr = nullptr;
                try {
                    auto yInitial = ptrCast<YieldData *>(data); // safe cast
                    void *value = thiz.unpackYieldData(*yInitial);
                    thiz.start(value);

                    ut_dcheck(thiz.mState == ST_Started &&
                        "Coroutine may not absorb ForcedUnwind exception");
                } catch (const ForcedUnwind&) {
                    ut_assert(thiz.mState == ST_Interrupting);
                } catch (...) {
                    ut_dcheck(thiz.mState == ST_Started &&
                        "Coroutine may not absorb ForcedUnwind exception");

#ifndef __GNUC__    // This check is disabled for now due to bugs in libstdc++.
                    ut_dcheck(!uncaughtException() &&
                            "May not throw from coroutine while another exception is propagating");
#endif

                    peptr = new Error(currentException());
                }

                // All remaining objects on stack have trivial destructors, coroutine is
                // considered unwinded.
                thiz.mState = ST_Done;

                try {
                    if (peptr != nullptr) {
                        ut_assert(*peptr != nullptr);
                        thiz.yieldException_(peptr);
                    } else {
                        thiz.yield_(nullptr);
                    }
                    ut_assert(false); // Can't resume unwinded coroutine.
                } catch (...) {
                    ut_assert(false); // Can't resume unwinded coroutine.
                }
            }

            static void* jump(CoroutineImplBase& from, CoroutineImplBase& to,
                const YieldData& yData)
            {
                ut_assert(&from != &to);
                ut_assert(!to.isDone());

                auto yReceived = reinterpret_cast<const YieldData*>( // safe cast
                    boost::context::jump_fcontext(
                        &from.mFContext, to.mFContext,
                        reinterpret_cast<intptr_t>(&yData), // safe cast
                        true)); // Suspend.

                return unpackYieldData(*yReceived);
            }

            static void* unpackYieldData(const YieldData& yReceived)
            {
                if (yReceived.kind == YK_Exception) {
                    auto peptr = static_cast<Error*>(yReceived.value); // safe cast

                    ut_assert(peptr != nullptr);
                    ut_assert(*peptr != nullptr);

                    auto eptr = Error(*peptr);
                    delete peptr;

                    rethrowException(eptr);
                    return nullptr;
                } else {
                    ut_assert(yReceived.kind == YK_Result);
                    return yReceived.value;
                }
            }

            void* yield_(const YieldData& yData)
            {
                ut_assert(context::callChainSize() > 1);
                ut_assert(context::currentCoroutine() == this);

                ut_assert(mState != ST_NotStarted);

                ut_dcheck(mState != ST_Interrupting &&
                    "Coroutine may not absorb ForcedUnwind exception");

                context::popCoroutine();
                auto& parent = *context::currentCoroutine();

                return jump(*this, parent, yData); // Suspend.
            }

            void forceUnwind() _ut_noexcept
            {
                ut_assert(mState == ST_Started);

                auto *parent = context::currentCoroutine();
                context::pushCoroutine(this);
                mState = ST_Interrupting;

                // Unwind.
                try {
                    jump(*parent, *this, YieldData(YK_Exception,
                        new Error(ForcedUnwind::ptr())));
                } catch (...) {
                    ut_assert(false);
                }

                ut_assert(mState == ST_Done);
                ut_assert(parent == context::currentCoroutine());
            }

            State mState;
            void *mValue;
            void *mFunction;
            boost::context::fcontext_t mFContext;
        };

        template <class F, class StackAllocator>
        class CoroutineImpl : public CoroutineImplBase
        {
        public:
            template <class U>
            CoroutineImpl(U&& f, StackAllocator stackAllocator, boost::context::stack_context stackContext, void *sp, size_t size) _ut_noexcept
                : CoroutineImplBase(sp, size)
                , mF(std::forward<U>(f))
                , mStackAllocator(std::move(stackAllocator))
                , mStackContext(stackContext)
            {
                static_assert(IsNoThrowCopyable<StackAllocator>::value,
                    "StackAllocator should be no-throw copyable. Consider adding noexcept or throw() specifier");

                static_assert(IsNoThrowCopyable<boost::context::stack_context>::value,
                    "Expecting boost::context::stack_context to be no-throw copyable");

                initialize(&mF);
            }

            void deallocate() _ut_noexcept final
            {
                StackAllocator stackAllocator = std::move(mStackAllocator);
                boost::context::stack_context stackContext = mStackContext;

                this->~CoroutineImpl();
                stackAllocator.deallocate(stackContext);
            }

            F& function()
            {
                return mF;
            }

        protected:
            void start(void *arg) final
            {
                CoroutineImplBase::start(arg);
                CoroutineFunctionTraits<F>::call(mF, arg);
            }

        private:
            F mF;
            StackAllocator mStackAllocator;
            boost::context::stack_context mStackContext;
        };

        namespace context
        {
            inline void initialize()
            {
                // Must be called from main stack.

                struct DummyCoroutineImpl : CoroutineImplBase
                {
                    void deallocate() _ut_noexcept final { }
                } static sMainCoroutine;

                pushCoroutine(&sMainCoroutine);

                // Initialize some eptrs in advance to avoid problems with
                // currentException() during exception propagation
                ForcedUnwind::ptr();
            }
        }
    }
}

}

#endif // UT_DISABLE_EXCEPTIONS
