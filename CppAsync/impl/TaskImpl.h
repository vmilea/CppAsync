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
#include "Assert.h"
#include "../util/AllocElementPtr.h"
#include "../util/Instance.h"
#include "../util/Misc.h"
#include "../util/TypeTraits.h"

namespace ut {

namespace detail
{
    template <class Listener>
    struct TaskListenerTraits
    {
        static_assert(HasResultType<Listener>::value,
            "Listener must derive from ut::ITaskListener<R>");

        using result_type = typename Listener::result_type;

        static_assert(std::is_base_of<ITaskListener<result_type>, Listener>::value,
            "Listener must derive from ut::ITaskListener<R>");

    private:
        using erased_listener_type = typename Task<result_type>::erased_listener_type;

        static_assert(sizeof(Listener) <= sizeof(erased_listener_type),
            "Listener type is too large");

        static_assert(
            std::alignment_of<Listener>::value <= std::alignment_of<erased_listener_type>::value,
            "Listener type alignment is too large");

        static_assert(std::is_nothrow_move_constructible<Listener>::value,
            "Listener type may not throw from move constructor. "
            "Consider adding noexcept or throw() specifier");
    };

    //
    // Common listener types
    //

    template <class R>
    struct DefaultListener
        : TaskListenerMixin<DefaultListener, R>
    {
        DefaultListener() _ut_noexcept { }

        DefaultListener(DefaultListener&& /* other */) _ut_noexcept { }

        void onDetach() _ut_noexcept final
        {
            ut_check(false && "Task doesn't support detachment");
        }

        void onDone(Task<R>& /* task */) _ut_noexcept final
        {
            // nothing
        }
    };

    template <class T>
    struct GenericReset
    {
        void operator()(T& resource) const _ut_noexcept
        {
            genericReset(resource);
        }
    };

    template <class T>
    struct DetachByReleasing
    {
        void operator()(T& resource) const _ut_noexcept
        {
            resource.release();
        }
    };

    template <class T>
    struct DetachNotSupported
    {
        void operator()(T& /* resource */) const _ut_noexcept
        {
            ut_check(false && "Task doesn't support detachment");
        }
    };

    template <class R, class T,
        class Detacher = DetachNotSupported<T>, class Resetter = GenericReset<T>>
    struct BoundResourceListener
        : TaskListenerMixin<BoundResourceListener, R, T, Detacher, Resetter>
    {
        explicit BoundResourceListener(T&& resource) _ut_noexcept
            : resource(std::move(resource))
        {
            static_assert(std::is_rvalue_reference<T&&>::value,
                "Argument expected to be an rvalue");

            static_assert(std::is_nothrow_move_constructible<T>::value,
                "Resource type may not throw from move constructor. Consider adding noexcept or throw() specifier");
        }

        BoundResourceListener(BoundResourceListener&& other) _ut_noexcept
            : resource(std::move(other.resource)) { }

        void onDetach() _ut_noexcept final
        {
            Detacher()(resource);
        }

        void onDone(Task<R>& /* task */) _ut_noexcept final
        {
            Resetter()(resource);
        }

        T resource;
    };

    template <class R, class T>
    using TaskMaster = BoundResourceListener<R, T, DetachByReleasing<T>, GenericReset<T>>;

    //
    // Type erasure for awaitables
    //

    template <class Awaitable>
    struct IgnoreCancellation
    {
        void operator()(Awaitable& /* awt */) _ut_noexcept { }
    };

    template <class Awaitable, class CancellationHandler>
    class AsTaskWrapper
        : public Awaiter
        , private CancellationHandler // Allow empty base class optimization.
    {
        using result_type = AwaitableResult<Awaitable>;
        using promise_type = Promise<result_type>;

    public:
        AsTaskWrapper(Awaitable& core, CancellationHandler&& cancellationHandler) _ut_noexcept
            : CancellationHandler(std::move(cancellationHandler))
            , mCore(core)
        {
            ut_assert(!awaitable::isReady(mCore));
            awaitable::setAwaiter(mCore, this);
        }

        void initialize(promise_type&& promise) _ut_noexcept
        {
            mPromise.initialize(std::move(promise));
        }

        ~AsTaskWrapper() _ut_noexcept final
        {
            if (mPromise->state() == promise_type::ST_OpCanceled)
                (*this)(mCore);
        }

        void resume(AwaitableBase *resumer) _ut_noexcept final
        {
            ut_assert(resumer == &mCore);
            ut_assert(awaitable::isReady(mCore));

            if (awaitable::hasError(mCore))
                mPromise->fail(awaitable::takeError(mCore));
            else
                complete<result_type>();
        }

    private:
        AsTaskWrapper(const AsTaskWrapper& other) = delete;
        AsTaskWrapper& operator=(const AsTaskWrapper& other) = delete;

        template <class U, EnableIfVoid<U> = nullptr>
        void complete() _ut_noexcept
        {
            mPromise->complete();
        }

        template <class U, DisableIfVoid<U> = nullptr>
        void complete() _ut_noexcept
        {
            mPromise->complete(awaitable::takeResult(mCore));
        }

        Instance<promise_type> mPromise;
        Awaitable& mCore;
    };

    template <class R, class Awaitable, EnableIfVoid<R> = nullptr>
    void loadResult(Promise<R>&& promise, Awaitable& awt)
    {
        ut_assert(awaitable::isReady(awt));

        if (awaitable::hasError(awt))
            promise.fail(awaitable::takeError(awt));
        else
            promise.complete();
    }

    template <class R, class Awaitable, DisableIfVoid<R> = nullptr>
    void loadResult(Promise<R>&& promise, Awaitable& awt)
    {
        ut_assert(awaitable::isReady(awt));

        if (awaitable::hasError(awt))
            promise.fail(awaitable::takeError(awt));
        else
            promise.complete(awaitable::takeResult(awt));
    }

    template <class Awaitable, class CancellationHandler, class Alloc>
    Task<AwaitableResult<Awaitable>> asTaskImpl(std::allocator_arg_t, const Alloc& alloc,
        Awaitable& awt, CancellationHandler&& cancellationHandler)
    {
        using result_type = AwaitableResult<Awaitable>;
        using awaiter_type = AsTaskWrapper<Awaitable, CancellationHandler>;
        using awaiter_handle_type = AllocElementPtr<awaiter_type, Alloc>;
        using listener_type = BoundResourceListener<result_type, awaiter_handle_type>;

        if (awaitable::isReady(awt)) {
            ut::Task<result_type> task;
            loadResult(task.takePromise(), awt);

            return task;
        } else {
            awaiter_handle_type handle(alloc, awt, std::move(cancellationHandler));

#ifdef UT_NO_EXCEPTIONS
            if (handle == nullptr) {
                Task<result_type> task;
                task.takePromise();
                return task; // Return invalid task.
            }
#endif

            auto task = makeTaskWithListener<listener_type>(std::move(handle));
            auto& awaiter = *task.template listenerAs<listener_type>().resource;
            awaiter.initialize(task.takePromise());

            return task;
        }
    }
}

}
