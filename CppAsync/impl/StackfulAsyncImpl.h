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

#ifndef UT_NO_EXCEPTIONS

#include "Assert.h"
#include "../util/Cast.h"
#include "../util/Instance.h"
#include "../util/StashFunction.h"
#include "../StackfulCoroutine.h"

namespace ut {

namespace detail
{
    namespace stackful
    {
        //
        // Function validation -- for ut::stackful::startAsync(lambda)
        //

        template <class F>
        struct AsyncFunctionTraits
        {
            static_assert(std::is_rvalue_reference<F&&>::value,
                "Stackful startAsync() expects an rvalue to the coroutine function");

            static_assert(IsFunctor<F>::value,
                "Stackful startAsync() expects function signature: "
                "R f() or R f(ut::stackful::Context<R>)");

            static_assert(FunctionHasArityLessThan2<F>::value,
                "Stackful startAsync() expects function signature: "
                "R f() or R f(ut::stackful::Context<R>)");

            using result_type = FunctionResult<F>;

            template <class T, EnableIf<std::is_void<FunctionResult<T>>::value
                && FunctionIsNullary<T>::value> = nullptr>
            static void call(T&& f, void * /* outResult */)
            {
                f();
            }

            template <class T, EnableIf<!std::is_void<FunctionResult<T>>::value
                && FunctionIsNullary<T>::value> = nullptr>
            static void call(T&& f, void *outResult)
            {
                new (outResult) result_type(f());
            }

            template <class T, EnableIf<std::is_void<FunctionResult<T>>::value
                && FunctionIsUnary<T>::value> = nullptr>
            static void call(T&& f, void * /* outResult */)
            {
                static_assert(std::is_same<Context<result_type>, FunctionArg<T, 0>>::value,
                    "Stackful startAsync() expects function signature: "
                    "R f() or R f(ut::stackful::Context<R>)");

                f(Context<result_type>());
            }

            template <class T, EnableIf<!std::is_void<FunctionResult<T>>::value
                && FunctionIsUnary<T>::value> = nullptr>
            static void call(T&& f, void *outResult)
            {
                static_assert(std::is_same<Context<result_type>, FunctionArg<T, 0>>::value,
                    "Stackful startAsync() expects function signature: "
                    "R f() or R f(ut::stackful::Context<R>)");

                new (outResult) result_type(f(Context<result_type>()));
            }
        };

        template <class R>
        class AsyncCoroutineAwaiter;

        //
        // Async function wrapper
        //

        template <class F>
        class AsyncCoroutineFunction
            : public StashFunctionBase<AsyncCoroutineAwaiter<FunctionResult<F>>>
        {
        public:
            using base_type = StashFunctionBase<AsyncCoroutineAwaiter<FunctionResult<F>>>;

            AsyncCoroutineFunction(F&& f)
                : mF(std::move(f))
            {
                static_assert(std::is_rvalue_reference<F&&>::value, "");
            }

            void operator()()
            {
                AsyncFunctionTraits<F>::call(mF, &this->stash().mResultData);
                this->stash().mHasResult = true;
            }

        private:
            AsyncCoroutineFunction(const AsyncCoroutineFunction& other) = delete;
            AsyncCoroutineFunction& operator=(const AsyncCoroutineFunction& other) = delete;

            F mF;
        };

        //
        // Coroutine manager
        //

        class AsyncCoroutineAwaiterBase : public Awaiter
        {
        public:
            virtual PromiseBase& promise() _ut_noexcept = 0;
        };

        template <class R>
        class AsyncCoroutineAwaiter : public AsyncCoroutineAwaiterBase
        {
        public:
            AsyncCoroutineAwaiter() _ut_noexcept
                : mCoroutine(nullptr)
                , mHasResult(false) { }

            ~AsyncCoroutineAwaiter() _ut_noexcept
            {
                if (mHasResult)
                    result().~adapted_result_type();

                auto state = mPromise->state();

                switch (state)
                {
                case PromiseBase::ST_Moved:
                    // Promise has been taken over by user.
                    break;
                case PromiseBase::ST_OpDone:
                    // Coroutine has completed or failed.
                    break;
                case PromiseBase::ST_OpRunning:
                    ut_dcheck(false &&
                        "Stackful coroutine may not delete itself while it is executing");
                    break;
                case PromiseBase::ST_OpRunningDetached:
                    // ST_Moved expected instead after completing a detached coroutine.
                    ut_assert(false);
                    break;
                case PromiseBase::ST_OpCanceled:
                    // Attached task has been deleted, dragging coroutine with it.
                    break;
                default:
                    ut_assert(false);
                    break;
                }
            }

            void resume(AwaitableBase *resumer) _ut_noexcept final
            {
                execute(resumer);
            }

            Promise<R>& promise() _ut_noexcept final
            {
                return *mPromise;
            }

            void start(CoroutineImplBase *coroutine, Promise<R>&& promise) _ut_noexcept
            {
                mCoroutine = coroutine;
                mPromise.initialize(std::move(promise));

                execute(nullptr);
            }

        private:
            AsyncCoroutineAwaiter(const AsyncCoroutineAwaiter& other) = delete;
            AsyncCoroutineAwaiter& operator=(const AsyncCoroutineAwaiter& other) = delete;

            using adapted_result_type = Replace<R, void, Nothing>;
            using result_storage_type = StorageFor<adapted_result_type>;

            adapted_result_type& result() _ut_noexcept
            {
                ut_assert(mHasResult);

                return ptrCast<adapted_result_type&>(mResultData); // safe cast
            }

            void execute(AwaitableBase *resumer) _ut_noexcept
            {
                ut_dcheck(mPromise->state() != PromiseBase::ST_Moved &&
                    "Async coroutine may not be resumed after taking over promise");

                ut_assert(mPromise->state() == PromiseBase::ST_OpRunning
                    || mPromise->state() == PromiseBase::ST_OpRunningDetached);

                Error eptr;
                try {
                    (*mCoroutine)(resumer);
                } catch (...) {
                    eptr = currentException();
                }

                auto state = mPromise->state();

                if (mCoroutine->isDone()) {
                    if (eptr == nullptr) {
                        ut_assert(mHasResult);

                        switch (state)
                        {
                        case PromiseBase::ST_Moved:
                            // Task has been taken over by user, nothing to do
                            break;
                        case PromiseBase::ST_OpDone:
                            // If CoroutineAwaiter still owns the promise, it cannot have completed
                            // (no need to account for ut_return() macro in stackful coroutines).
                            ut_assert(false);
                            break;
                        case PromiseBase::ST_OpRunning:
                            // Complete the promise. TaskMaster will be notified and deallocate
                            // the coroutine.
                            complete(*mPromise);
                            break;
                        case PromiseBase::ST_OpRunningDetached:
                            // There is no task attached. Put promise in a neutral state.
                            { Promise<R> tmpPromise(std::move(*mPromise)); }
                            // Deallocate self.
                            mCoroutine->deallocate();
                            break;
                        case PromiseBase::ST_OpCanceled:
                            // Tasks aren't supposed to delete themselves from within coroutine.
                            ut_assert(false);
                            break;
                        default:
                            ut_assert(false);
                            break;
                        }
                    } else {
                        switch (state)
                        {
                        case PromiseBase::ST_Moved:
                            ut_dcheck(false &&
                                "May not throw from async coroutine after taking over promise");
                            break;
                        case PromiseBase::ST_OpDone:
                            ut_dcheck(false &&
                                "May not throw from async coroutine after completing promise");
                            break;
                        case PromiseBase::ST_OpRunning:
                            // Complete the promise. TaskMaster will be notified and deallocate
                            // the coroutine.
                            mPromise->fail(eptr);
                            break;
                        case PromiseBase::ST_OpRunningDetached:
                            try {
                                rethrowException(eptr);
                            } catch (const std::exception& e) {
                                fprintf(stderr, "[CPP-ASYNC] UNCAUGHT EXCEPTION: %s\n", e.what());
                            } catch (...) {
                                fprintf(stderr, "[CPP-ASYNC] UNCAUGHT EXCEPTION\n");
                            }
                            std::terminate();
                            break;
                        case PromiseBase::ST_OpCanceled:
                            // Tasks aren't supposed to delete themselves from within coroutine.
                            ut_assert(false);
                            break;
                        default:
                            ut_assert(false);
                            break;
                        }
                    }
                } else {
                    ut_dcheck(state != PromiseBase::ST_Moved &&
                        "Async coroutine must return immediately after taking over promise. "
                        "No further suspension allowed");

                    ut_assert(state == PromiseBase::ST_OpRunning
                        || state == PromiseBase::ST_OpRunningDetached);
                }
            }

            template <class U, EnableIfVoid<U> = nullptr>
            void complete(Promise<U>& promise) _ut_noexcept
            {
                promise.complete();
            }

            template <class U, DisableIfVoid<U> = nullptr>
            void complete(Promise<U>& promise) _ut_noexcept
            {
                promise.complete(std::move(result()));
            }

            CoroutineImplBase *mCoroutine;
            Instance<Promise<R>> mPromise;
            bool mHasResult;
            result_storage_type mResultData;

            template <class F>
            friend class AsyncCoroutineFunction;
        };

        //
        // Context
        //

        namespace context
        {
            inline void* currentStashPtr() _ut_noexcept
            {
                ut_dcheck(callChainSize() > 1 &&
                    "No coroutine active, can't access stash");

                return currentCoroutine()->functionPtr();
            }

            inline AsyncCoroutineAwaiterBase& currentStash() _ut_noexcept
            {
                return *static_cast<AsyncCoroutineAwaiterBase*>(currentStashPtr()); // safe cast
            }
        }

        inline void checkAwaitConditions() _ut_noexcept
        {
            ut_dcheck(context::callChainSize() > 1 &&
                "Only stackful coroutines may call ut::await_()."
                "Stackless coroutines should use the ut_await_() macro instead.");

            PromiseBase& promise = context::currentStash().promise();
            (void) promise;

            ut_dcheck(promise.state() != PromiseBase::ST_Moved &&
                "May not await after taking promise");

            ut_assert(promise.isCompletable());
        }

        //
        // Stackful await - range overloads
        //

        template <class Awaitable>
        void awaitImpl_(Awaitable& awt)
        {
            checkAwaitConditions();

            if (!awaitable::isReady(awt)) {
                awaitable::setAwaiter(awt, &context::currentStash());

                // yield_() may throw ut::ForcedUnwind.
                void *doneAwt = yield_();
                (void) doneAwt; // Suppress unused-variable warning.

                ut_assert(doneAwt == nullptr || doneAwt == &awt);
                ut_assert(awaitable::isReady(awt));
            }
        }

        template <class It>
        AwaitableBase* rAwaitAnyNoThrow_(Range<It> range)
        {
            using namespace ops;

            checkAwaitConditions();

            ut_dcheck(rAllValid(range) &&
                "Can't await invalid objects");

            auto pos = rFind<isReady>(range);
            if (pos != range.last)
                return &selectAwaitable(*pos);

            rSetAwaiter(&context::currentStash(), range);

            // yield_() may throw ut::ForcedUnwind.
            auto *doneAwt = static_cast<AwaitableBase*>(yield_()); // safe cast

            ut_assert(doneAwt != nullptr);
            ut_assert(doneAwt->isReady());
            ut_assert(&selectAwaitable(*rFind<isReady>(range)) == doneAwt);

            rSetAwaiter(nullptr, range);

            return doneAwt;
        }

        template <class It>
        AwaitableBase* rAwaitAny_(Range<It> range)
        {
            AwaitableBase *doneAwt = rAwaitAnyNoThrow_(range);

            if (doneAwt->hasError())
                rethrowException(doneAwt->error());

            return doneAwt;
        }

        template <class It>
        AwaitableBase* rAwaitAllNoThrow_(Range<It> range)
        {
            using namespace ops;

            checkAwaitConditions();

            ut_dcheck(rAllValid(range) &&
                "Can't await invalid objects");

            std::size_t count = 0;

            for (auto& item : range) {
                AwaitableBase& awt = selectAwaitable(item);

                if (awt.isReady()) {
                    if (awt.hasError())
                        return &awt;
                } else {
                    count++;
                }
            }

            if (count == 0)
                return nullptr;

            rSetAwaiter(&context::currentStash(), range);

            do {
                // yield_() may throw ut::ForcedUnwind.
                auto *doneAwt = static_cast<AwaitableBase*>(yield_()); // safe cast

                ut_assert(doneAwt != nullptr);
                ut_assert(doneAwt->isReady());
                ut_assert(rIsAnyOf(doneAwt, range));
                ut_assert(doneAwt->hasError() || !rAny<hasError>(range));

                if (doneAwt->hasError()) {
                    rSetAwaiter(nullptr, range);
                    return doneAwt;
                }
            } while (--count > 0);

            ut_assert(rNone<hasAwaiter>(range));
            return nullptr;
        }

        template <class It>
        void rAwaitAll_(Range<It> range)
        {
            AwaitableBase *failedAwt = rAwaitAllNoThrow_(range);

            if (failedAwt != nullptr)
                rethrowException(failedAwt->error());
        }
    }
}

}

#endif // UT_NO_EXCEPTIONS
