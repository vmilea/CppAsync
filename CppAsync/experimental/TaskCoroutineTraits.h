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

#if defined(_MSC_VER) && _MSC_VER >= 1900

#include "../impl/Common.h"
#include "../util/Instance.h"
#include "../util/Cast.h"
#include "../Task.h"
#include <experimental/resumable>

namespace ut {

namespace detail
{
    namespace experimental
    {
        using AwaitContext = MaxAlignedStorage<2 * ptr_size>;
        using ContextCleaner = void (*)(AwaitContext *awaitContext);

        template <class R>
        struct TaskPromiseMixin;

        template <class R>
        class TaskPromise : public TaskPromiseMixin<R>
        {
        public:
            using result_type = R;
            using coroutine_handle_type = std::experimental::coroutine_handle<TaskPromise>;

            static TaskPromise& fromAwaitContext(void *awaitContext) _ut_noexcept
            {
                return *static_cast<TaskPromise*>(awaitContext);
            }

            TaskPromise() _ut_noexcept
                : mContextCleaner(nullptr) { }

            ~TaskPromise() _ut_noexcept
            {
                if (mContextCleaner != nullptr)
                    mContextCleaner(&mAwaitContext);
            }

            Task<R> get_return_object() _ut_noexcept
            {
                auto task = ut::makeTaskWithListener<Listener>(this);
                mPromise.initialize(task.takePromise());

                resumeCoroutine();

                return std::move(task);
            }

            bool initial_suspend() const _ut_noexcept
            {
                return true;
            }

            bool final_suspend() const _ut_noexcept
            {
                return false;
            }

            // Ignored in MSVC 14.0
            //
            // void set_exception(ut::Error error)
            // {
            //     mPromise->fail(std::move(error));
            // }

            AwaitContext* replaceAwaitContext(ContextCleaner contextCleaner) _ut_noexcept
            {
                if (mContextCleaner != nullptr)
                    mContextCleaner(&mAwaitContext);

                mContextCleaner = contextCleaner;

                return &mAwaitContext;
            }

            void resumeCoroutine() _ut_noexcept
            {
                ut_assert(mPromise->state() == PromiseBase::ST_OpRunning
                    || mPromise->state() == PromiseBase::ST_OpRunningDetached);

                auto coro = coroutine_handle_type::from_promise(this);

#ifdef UT_NO_EXCEPTIONS
                coro.resume();
#else
                Error eptr;
                try {
                    coro.resume();
                    return;
                } catch (...) {
                    eptr = currentException();
                }
                interruptCoroutine(std::move(eptr));
#endif
            }

            void interruptCoroutine(Error error) _ut_noexcept
            {
                Promise<R> promise(std::move(*mPromise));
                coroutine_handle_type::from_promise(this).destroy();

                if (promise.isCompletable())
                    promise.fail(std::move(error));
            }

        private:
            TaskPromise(const TaskPromise& other) = delete;
            TaskPromise& operator=(const TaskPromise& other) = delete;

            class Listener : public UniqueMixin<ITaskListener<R>, Listener>
            {
            public:
                explicit Listener(TaskPromise<R> *taskPromise) _ut_noexcept
                    : mTaskPromise(taskPromise) { }

                Listener(Listener&& other) _ut_noexcept
                    : mTaskPromise(movePtr(other.mTaskPromise)) { }

                ~Listener() _ut_noexcept final
                {
                    if (mTaskPromise != nullptr) {
                        // Interrupt coroutine.

                        Promise<R>& promise = *mTaskPromise->mPromise;

                        ut_dcheck(!promise.isCompletable() &&
                            "Stackless coroutine may not delete itself while it is executing");

                        ut_assert(promise.state() == ut::PromiseBase::ST_OpCanceled);

                        coroutine_handle_type::from_promise(mTaskPromise).destroy();
                    }
                }

                void onDetach() _ut_noexcept final
                {
                    mTaskPromise = nullptr;
                }

                void onDone(Task<R>& task) _ut_noexcept final
                {
                    mTaskPromise = nullptr;
                }

            private:
                TaskPromise<R> *mTaskPromise;
            };

            AwaitContext mAwaitContext;
            ContextCleaner mContextCleaner;
            Instance<Promise<R>> mPromise;

            friend struct TaskPromiseMixin<R>;
        };

        template <class R>
        struct TaskPromiseMixin
        {
            template <class U>
            void return_value(U&& value) _ut_noexcept
            {
                auto& thiz = static_cast<TaskPromise<R>&>(*this);

                if (thiz.mPromise->isCompletable())
                    thiz.mPromise->complete(std::forward<U>(value));
            }
        };

        template <>
        struct TaskPromiseMixin<void>
        {
            void return_void() _ut_noexcept
            {
                auto& thiz = static_cast<TaskPromise<void>&>(*this);

                if (thiz.mPromise->isCompletable())
                    thiz.mPromise->complete();
            }
        };
    }
}

}

namespace std {

namespace experimental
{
    template <class R, class ...Args>
    struct coroutine_traits<ut::Task<R>, Args...>
    {
        using promise_type = ut::detail::experimental::TaskPromise<R>;

        template <class Alloc, class ...Args>
        static Alloc get_allocator(std::allocator_arg_t, const Alloc& alloc, Args&&...)
        {
            return alloc;
        }

#ifdef UT_NO_EXCEPTIONS
        static ut::Task<R> get_return_object_on_allocation_failure() _ut_noexcept
        {
            ut::Task<R> task;
            ut::Task<R> tmp = std::move(task);
            return task; // Return an invalid task.
        }
#endif
    };
}

}

namespace ut
{
    bool await_ready(AwaitableBase& awt) _ut_noexcept
    {
        ut_dcheck(awt.isValid() &&
            "Can't await invalid objects");

#ifdef UT_NO_EXCEPTIONS
        return awt.hasError()
            // Suspend and destroy coroutine, then store error in Task.
            ? false
            // Suspend if result is not yet available.
            : awt.isReady();
#else
        // Suspend if result is not yet available.
        return awt.isReady();
#endif
    }

    template <class CoroutineResult>
    void await_suspend(AwaitableBase& awt, std::experimental::coroutine_handle<
        detail::experimental::TaskPromise<CoroutineResult>> coro) _ut_noexcept
    {
        using task_promise_type = detail::experimental::TaskPromise<CoroutineResult>;

        struct Awaiter : ut::Awaiter
        {
            void resume(AwaitableBase *resumer) _ut_noexcept final
            {
                auto& taskPromise = task_promise_type::fromAwaitContext(this);

#ifdef UT_NO_EXCEPTIONS
                if (resumer->hasError())
                    // Destroy coroutine, then store error in Task.
                    taskPromise.interruptCoroutine(std::move(resumer->error()));
                else
                    // Resume and take result.
                    taskPromise.resumeCoroutine();
#else
                (void) resumer;
                // Resume and take result or throw exception.
                taskPromise.resumeCoroutine();
#endif
            }
        };

        auto& taskPromise = coro.promise();

#ifdef UT_NO_EXCEPTIONS
        if (awt.hasError()) {
            // Destroy coroutine, then store error in Task.
            taskPromise.interruptCoroutine(std::move(awt.error()));
            return;
        }
#endif

        auto *awaiter = new (taskPromise.replaceAwaitContext(nullptr)) Awaiter();
        awt.setAwaiter(awaiter);

        // No need for cleanup.
        //
        // auto cleaner = [](detail::experimental::AwaitContext *awaitContext) {
        //     ptrCast<Awaiter *>(awaitContext)->~Awaiter();
        // });
    }

    template <class R, DisableIfVoid<R> = nullptr>
    R&& await_resume(detail::CommonAwaitable<R>& awt)
    {
        return std::move(awt.get());
    }

    template <class R, EnableIfVoid<R> = nullptr>
    void await_resume(detail::CommonAwaitable<R>& awt)
    {
        awt.get();
    }
}

#endif
