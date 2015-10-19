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
#include "../util/Instance.h"
#include "../util/Meta.h"
#include "../StacklessCoroutine.h"
#include "../Task.h"
#include "AwaitableOps.h"

#ifdef UT_DISABLE_EXCEPTIONS

#define _ut_check_for_error(awt) \
    if (ut::detail::awaitable::hasError(awt)) { \
        this->ut_asyncState.promise->fail(ut::detail::awaitable::takeError(awt)); \
        return; \
    }

#else

#define _ut_check_for_error(awt) \
    if (ut::detail::awaitable::hasError(awt)) \
        ut::rethrowException(ut::detail::awaitable::takeError(awt));

#endif // UT_DISABLE_EXCEPTIONS

namespace ut {

namespace stackful
{
    template <class T>
    class BasicStackAllocator;
}

namespace detail
{
    namespace stackless
    {
        template <class R>
        struct AsyncFrameState
        {
            Instance<Promise<R>> promise;
            Awaiter *self;
            void *arg;

            AsyncFrameState(void *startupArg) _ut_noexcept
                : self(nullptr)
                , arg(startupArg) { }

            AwaitableBase& resumer()
            {
                return *static_cast<AwaitableBase*>(arg); // safe cast
            }
        };

        //
        // Frame validation
        //

        template <class CustomFrame>
        struct AsyncFrameTraits
        {
            static_assert(HasResultType<CustomFrame>::value,
                "Frame must derive from ut::AsyncFrame<R>");

            using result_type = typename CustomFrame::result_type;

            static_assert(std::is_base_of<AsyncFrame<result_type>, CustomFrame>::value,
                "Frame must derive from ut::AsyncFrame<R>");

            static_assert(IsFunctor<CustomFrame>::value,
                "Frame must be a functor with signature: void operator()()");

            static_assert(FunctionIsNullary<CustomFrame>::value,
                "Frame must be a functor with signature: void operator()()");
        };

        //
        // Task coroutine
        //

        template <class CustomFrame, class Allocator>
        struct AsyncCoroutineAwaiter : Awaiter
        {
            StacklessCoroutine<CustomFrame> coroutine;

            using result_type = typename AsyncFrameTraits<CustomFrame>::result_type;
            using handle_type = AllocElementPtr<AsyncCoroutineAwaiter, Allocator>;

            template <class ...Args>
            AsyncCoroutineAwaiter(Args&&... frameArgs)
                : coroutine(std::forward<Args>(frameArgs)...) { }

            ~AsyncCoroutineAwaiter()
            {
                auto state = coroutine.frame().ut_asyncState.promise->state();

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
                        "Stackless coroutine may not delete itself while it is executing");
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

            void start() _ut_noexcept
            {
                execute();
            }

            void resume(AwaitableBase *resumer) _ut_noexcept final
            {
                coroutine.frame().ut_asyncState.arg = resumer;
                execute();
            }

        private:
            AsyncCoroutineAwaiter(const AsyncCoroutineAwaiter& other) = delete;
            AsyncCoroutineAwaiter& operator=(const AsyncCoroutineAwaiter& other) = delete;

            void execute() _ut_noexcept
            {
                Promise<result_type>& promise = *coroutine.frame().ut_asyncState.promise;

                ut_dcheck(promise.state() != PromiseBase::ST_Moved &&
                    "Async coroutine may not be resumed after taking over promise");

                ut_assert(promise.state() == PromiseBase::ST_OpRunning
                    || promise.state() == PromiseBase::ST_OpRunningDetached);

                PromiseBase::State state;

                switch (coroutine.resume().value)
                {
                case StacklessCoroutineStatus::SC_Done:
                    coroutine.frame().ut_asyncState.arg = nullptr;
                    state = promise.state();

                    if (isNil(context::loopbackException())) {
                        switch (state)
                        {
                        case PromiseBase::ST_Moved:
                            // Task has been taken over by user, nothing to do
                            break;
                        case PromiseBase::ST_OpDone:
                        case PromiseBase::ST_OpRunning:
                            // Complete the promise. TaskMaster will be notified and deallocate
                            // the coroutine.
                            finalize(promise);
                            break;
                        case PromiseBase::ST_OpRunningDetached:
                            // There is no task attached. Put promise in a neutral state.
                            { Promise<result_type> tmpPromise(std::move(promise)); }
                            // Deallocate self.
                            handle_type::restoreFromCore(*this).reset();
                            break;
                        case PromiseBase::ST_OpCanceled:
                            ut_assert(false); // received only on SC_Destructed branch
                            break;
                        default:
                            ut_assert(false);
                            break;
                        }
                    } else {
#ifdef UT_DISABLE_EXCEPTIONS
                        ut_assert(false);
#else
                        auto eptr = context::loopbackException();
                        reset(context::loopbackException());

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
                            promise.fail(eptr);
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
                            ut_assert(false); // received only on SC_Destructed branch
                            break;
                        default:
                            ut_assert(false);
                            break;
                        }
#endif
                    }
                    break;
                case StacklessCoroutineStatus::SC_Suspended:
                    coroutine.frame().ut_asyncState.arg = nullptr;
                    state = promise.state();

                    ut_dcheck(state != PromiseBase::ST_Moved &&
                        "Async coroutine must return immediately after taking over promise. "
                        "No further suspension allowed");

                    ut_assert(state == PromiseBase::ST_OpRunning
                        || state == PromiseBase::ST_OpRunningDetached);

                    break;
                case StacklessCoroutineStatus::SC_Destructed:
                    // Promise has been canceled or completed via ut_return(), causing immediate
                    // deallocation of coroutine. Quit without touching members.
                    break;
                default:
                    ut_assert(false);
                }
            }

            template <class T, EnableIfVoid<T> = nullptr>
            static void finalize(Promise<T>& promise) _ut_noexcept
            {
                // Task<void> gets completed implicitly after coroutine finish
                if (promise.isCompletable())
                    promise.complete();
            }

            template <class T, DisableIfVoid<T> = nullptr>
            static void finalize(Promise<T>& promise) _ut_noexcept
            {
                // Task<R> must be completed via ut_return(result)
                ut_dcheck(!promise.isCompletable() &&
                    "Async coroutine has finished without returning via ut_return()");
            }
        };

        //
        // Helpers for await macros
        //

        template <class Awaitable>
        bool awaitHelper0(Awaiter& awaiter, Awaitable& awt) _ut_noexcept
        {
            // ut_dcheck(awaitable::isValid(awt) && "Can't await invalid objects");

            if (awaitable::isReady(awt)) {
                return false;
            } else {
                awaitable::setAwaiter(awt, &awaiter);
                return true;
            }
        }

        template <class Awaitable>
        void awaitHelper1(void *resumer, Awaitable& awt) _ut_noexcept
        {
            ut_assert(resumer == nullptr || resumer == &awt);
            ut_assert(awaitable::isReady(awt));
        }

        template <class ...Awaitables>
        bool awaitAnyHelper0Impl(Awaiter& awaiter, AwaitableBase*& outdoneAwt,
            Awaitables&&... awts) _ut_noexcept
        {
            using namespace ops;

            ut_dcheck(allValid(awts...) &&
                "Can't await invalid objects");

            AwaitableBase *doneAwt = find<isReady>(awts...);

            if (doneAwt != nullptr) {
                outdoneAwt = doneAwt;
                return false;
            } else {
                setAwaiter(&awaiter, awts...);
                return true;
            }
        }

        template <class ...Awaitables>
        void awaitAnyHelper1Impl(AwaitableBase& resumer, AwaitableBase*& outdoneAwt,
            Awaitables&&... awts) _ut_noexcept
        {
            using namespace ops;

            ut_assert(resumer.isReady());
            ut_assert(find<isReady>(awts...) == &resumer);

            outdoneAwt = &resumer;
            setAwaiter(nullptr, awts...);
        }

        template <class ...Awaitables>
        bool awaitAll_Helper0Impl(Awaiter& awaiter, AwaitableBase*& outFailedAwt,
            Awaitables&&... awts) _ut_noexcept
        {
            using namespace ops;

            ut_dcheck(allValid(awts...) &&
                "Can't await invalid objects");

            AwaitableBase *failedAwt = find<hasError>(awts...);

            if (failedAwt != nullptr) {
                outFailedAwt = failedAwt;
                return false;
            } else if (all<isReady>(awts...)) {
                outFailedAwt = nullptr;
                return false;
            } else {
                setAwaiter(&awaiter, awts...);
                return true;
            }
        }

        template <class ...Awaitables>
        bool awaitAll_Helper1Impl(AwaitableBase& resumer, AwaitableBase*& outFailedAwt,
            Awaitables&&... awts) _ut_noexcept
        {
            using namespace ops;

            ut_assert(resumer.isReady());
            ut_assert(isAnyOf(&resumer, awts...));
            ut_assert(resumer.hasError() || !any<hasError>(awts...));

            if (resumer.hasError()) {
                outFailedAwt = &resumer;
                setAwaiter(nullptr, awts...);
                return false;
            } else if (all<isReady>(awts...)) {
                outFailedAwt = nullptr;
                ut_assert(none<hasAwaiter>(awts...));
                return false;
            } else {
                return true;
            }
        }

        //
        // Wrappers to avoid variadic template explosion
        //

        template <class ...Awaitables>
        bool awaitAnyHelper0(Awaiter& awaiter, AwaitableBase*& outdoneAwt,
            Awaitables&&... awts) _ut_noexcept
        {
            static_assert(All<IsPlainAwtBaseReference<Awaitables>...>::value,
                "ut_await_any_() expects awts to be non-const lvalue references to AwaitableBase");

            return awaitAnyHelper0Impl(awaiter, outdoneAwt,
                static_cast<AwaitableBase&>(awts)...); // safe cast
        }

        template <class ...Awaitables>
        void awaitAnyHelper1(AwaitableBase& resumer, AwaitableBase*& outdoneAwt,
            Awaitables&&... awts) _ut_noexcept
        {
            return awaitAnyHelper1Impl(resumer, outdoneAwt,
                static_cast<AwaitableBase&>(awts)...); // safe cast
        }

        template <class ...Awaitables>
        bool awaitAll_Helper0(Awaiter& awaiter, AwaitableBase*& outFailedAwt,
            Awaitables&&... awts) _ut_noexcept
        {
            static_assert(All<IsPlainAwtBaseReference<Awaitables>...>::value,
                "ut_await_all_() expects awts to be non-const lvalue references to AwaitableBase");

            return awaitAll_Helper0Impl(awaiter, outFailedAwt,
                static_cast<AwaitableBase&>(awts)...); // safe cast
        }

        template <class ...Awaitables>
        bool awaitAll_Helper1(AwaitableBase& resumer, AwaitableBase*& outFailedAwt,
            Awaitables&&... awts) _ut_noexcept
        {
            return awaitAll_Helper1Impl(resumer, outFailedAwt,
                static_cast<AwaitableBase&>(awts)...); // safe cast
        }
    }
}

}
