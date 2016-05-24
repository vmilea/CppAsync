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

#if defined(_MSC_VER) && _MSC_FULL_VER >= 190024120

#include "../impl/Common.h"
#include "../util/Cast.h"
#include "../Task.h"
#include <experimental/coroutine>

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

            static TaskPromise& fromAwaitContext(void *awaitContext) noexcept
            {
                return *static_cast<TaskPromise*>(awaitContext);
            }

            TaskPromise() noexcept
                : mContextCleaner(nullptr) { }

            ~TaskPromise() noexcept
            {
                if (mContextCleaner != nullptr)
                    mContextCleaner(&mAwaitContext);
            }

            Task<R> get_return_object() noexcept
            {
                auto task = ut::makeTaskWithListener<Listener>(this);
                mPromise = task.takePromise();

                resumeCoroutine();

                return task;
            }

#ifdef UT_NO_EXCEPTIONS
            static Task<R> get_return_object_on_allocation_failure() noexcept
            {
                Task<R> task;
                Task<R> tmp = std::move(task);
                return task; // Return an invalid task.
            }
#endif

            auto initial_suspend() const noexcept
            {
                return std::experimental::suspend_always();
            }

            auto final_suspend() const noexcept
            {
                return std::experimental::suspend_always();
            }

#ifndef UT_NO_EXCEPTIONS
            void set_exception(ut::Error error)
            {
                mPromise.fail(std::move(error));
            }
#endif

            AwaitContext* replaceAwaitContext(ContextCleaner contextCleaner) noexcept
            {
                if (mContextCleaner != nullptr)
                    mContextCleaner(&mAwaitContext);

                mContextCleaner = contextCleaner;

                return &mAwaitContext;
            }

            void resumeCoroutine() noexcept
            {
                ut_assert(mPromise.state() == PromiseBase::ST_OpRunning
                    || mPromise.state() == PromiseBase::ST_OpRunningDetached);

                auto coro = coroutine_handle_type::from_promise(*this);
                coro.resume();

                if (coro.done()) {
                    ut_assert(mPromise.state() == ut::PromiseBase::ST_OpDone);
                    coro.destroy();
                }
            }

            void interruptCoroutine(Error error) noexcept
            {
                Promise<R> promise(std::move(mPromise));
                coroutine_handle_type::from_promise(*this).destroy();

                if (promise.isCompletable())
                    promise.fail(std::move(error));
            }

        private:
            TaskPromise(const TaskPromise& other) = delete;
            TaskPromise& operator=(const TaskPromise& other) = delete;

            class Listener : public UniqueMixin<ITaskListener<R>, Listener>
            {
            public:
                explicit Listener(TaskPromise<R> *taskPromise) noexcept
                    : mTaskPromise(taskPromise) { }

                Listener(Listener&& other) noexcept
                    : mTaskPromise(movePtr(other.mTaskPromise)) { }

                ~Listener() noexcept final
                {
                    if (mTaskPromise != nullptr) {
                        // Interrupt coroutine.

                        ut_dcheck(!mTaskPromise->mPromise.isCompletable() &&
                            "Stackless coroutine may not delete itself while it is executing");

                        ut_assert(mTaskPromise->mPromise.state() == ut::PromiseBase::ST_OpCanceled);

                        coroutine_handle_type::from_promise(*mTaskPromise).destroy();
                    }
                }

                void onDetach() noexcept final
                {
                    mTaskPromise = nullptr;
                }

                void onDone(Task<R>& task) noexcept final
                {
                    mTaskPromise = nullptr;
                }

            private:
                TaskPromise<R> *mTaskPromise;
            };

            AwaitContext mAwaitContext;
            ContextCleaner mContextCleaner;
            Promise<R> mPromise;

            friend struct TaskPromiseMixin<R>;
        };

        template <class R>
        struct TaskPromiseMixin
        {
            template <class U>
            void return_value(U&& value) noexcept
            {
                auto& thiz = static_cast<TaskPromise<R>&>(*this);

                thiz.mPromise(std::forward<U>(value));
            }
        };

        template <>
        struct TaskPromiseMixin<void>
        {
            void return_void() noexcept
            {
                auto& thiz = static_cast<TaskPromise<void>&>(*this);

                thiz.mPromise();
            }
        };

        template <class R>
        struct CoAwaiterMixin;

        template <class R>
        class CoAwaiter : public CoAwaiterMixin<R>
        {
        public:
            CoAwaiter(CommonAwaitable<R>& awt)
                : mAwt(awt) { }

            bool await_ready() const noexcept
            {
                ut_dcheck(mAwt.isValid() &&
                    "Can't await invalid objects");

#ifdef UT_NO_EXCEPTIONS
                return mAwt.hasError()
                    // Suspend and destroy coroutine, then store error in Task.
                    ? false
                    // Suspend if result is not yet available.
                    : mAwt.isReady();
#else
                // Suspend if result is not yet available.
                return mAwt.isReady();
#endif
            }

            template <class CoroutineResult>
            void await_suspend(std::experimental::coroutine_handle<
                experimental::TaskPromise<CoroutineResult>> coro) noexcept
            {
                using task_promise_type = experimental::TaskPromise<CoroutineResult>;

                struct Awaiter : ut::Awaiter
                {
                    void resume(AwaitableBase *resumer) noexcept final
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
                if (mAwt.hasError()) {
                    // Destroy coroutine, then store error in Task.
                    taskPromise.interruptCoroutine(std::move(mAwt.error()));
                    return;
                }
#endif

                auto *awaiter = new (taskPromise.replaceAwaitContext(nullptr)) Awaiter();
                mAwt.setAwaiter(awaiter);

                // No need for cleanup.
                //
                // auto cleaner = [](experimental::AwaitContext *awaitContext) {
                //     ptrCast<Awaiter *>(awaitContext)->~Awaiter();
                // });
            }

        private:
            CommonAwaitable<R>& mAwt;

            friend struct CoAwaiterMixin<R>;
        };

        template <class R>
        struct CoAwaiterMixin
        {
            R&& await_resume()
            {
                auto& thiz = static_cast<CoAwaiter<R>&>(*this); // safe cast

                return thiz.mAwt.get();
            }
        };

        template <>
        struct CoAwaiterMixin<void>
        {
            void await_resume()
            {
                auto& thiz = static_cast<CoAwaiter<void>&>(*this); // safe cast

                thiz.mAwt.get();
            }
        };
    }

    template <class R>
    auto operator co_await(CommonAwaitable<R>& awt) noexcept
    {
        return experimental::CoAwaiter<R>(awt);
    }

    template <class R>
    auto operator co_await(CommonAwaitable<R>&& awt) noexcept // fine?
    {
        return experimental::CoAwaiter<R>(awt);
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
    };

    // Custom allocators are supported by specializing coroutine_traits according
    // to coroutine function signature. See CoroutineTraits.h for a sketch.
}

}

#endif
