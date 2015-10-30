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
#include "impl/Assert.h"
#include "impl/CommonAwaitable.h"
#include "util/Meta.h"
#include "util/SmartPtr.h"
#include "util/VirtualObject.h"
#include <exception>

namespace ut {

//
// Fwd declarations
//

namespace detail
{
    template <class Listener>
    struct TaskListenerTraits;
}

template <class R = void>
class Task;

template <class R = void>
class Promise;

template <class L, class ...Args>
auto makeTaskWithListener(Args&&... args) _ut_noexcept
    -> Task<typename detail::TaskListenerTraits<L>::result_type>;

//
// ITaskListener
//

template <class R = void>
struct ITaskListener : IVirtual
{
    using result_type = R;

    virtual void onDetach() _ut_noexcept = 0;

    virtual void onDone(Task<R>& /* task */) _ut_noexcept = 0;

protected:
    Task<R>& task() _ut_noexcept
    {
        return *ptrCast<Task<R>*>( // safe cast
            ptrCast<char*>(this) - sizeof(detail::CommonAwaitable<R>)); // safe cast
    }
};

template <template <class ...> class Derived, class R = void, class ...Ts>
using TaskListenerMixin = MovableMixin<ITaskListener<R>, Derived<R, Ts...>>;

}

#include "impl/TaskImpl.h"


namespace ut {

//
// Task
//

template <class R>
class PromiseMixin;

namespace detail
{
    template <class Listener>
    struct TaskListenerTraits;
}

template <class R>
class Task : public detail::CommonAwaitable<R>
{
    using adapted_result_type = Replace<R, void, Nothing>;

public:
    using result_type = R;

    Task() _ut_noexcept { }

    template <class Listener, class ...Args>
    Task(TypeInPlaceTag<Listener>, Args&&... args)
        : mListener(TypeInPlaceTag<Listener>(), std::forward<Args>(args)...) { }

    template <class U = R, EnableIf<
        std::is_same<U, R>::value && IsNoThrowMovable<adapted_result_type>::value> = nullptr>
    Task(Task<R>&& other) _ut_noexcept
        : Task(DelegateTag(), std::move(other)) { }

    template <class U = R, EnableIf<
        std::is_same<U, R>::value && !IsNoThrowMovable<adapted_result_type>::value> = nullptr>
    Task(Task<R>&& other)
        : Task(DelegateTag(), std::move(other)) { }

    template <class U = R, EnableIf<
        std::is_same<U, R>::value && IsNoThrowMovable<adapted_result_type>::value> = nullptr>
    Task& operator=(Task<R>&& other) _ut_noexcept
    {
        return assignImpl(std::move(other));
    }

    template <class U = R, EnableIf<
        std::is_same<U, R>::value && !IsNoThrowMovable<adapted_result_type>::value> = nullptr>
    Task& operator=(Task<R>&& other)
    {
        return assignImpl(std::move(other));
    }

    ~Task() _ut_noexcept
    {
        if (hasPromise())
            promise()->mState = Promise<R>::ST_OpCanceled;
    }

    template <class U = R, EnableIf<
        std::is_same<U, R>::value && IsNoThrowMovable<adapted_result_type>::value> = nullptr>
    void swap(Task<R>& other) _ut_noexcept
    {
        swapImpl(other);
    }

    template <class U = R, EnableIf<
        std::is_same<U, R>::value && !IsNoThrowMovable<adapted_result_type>::value> = nullptr>
    void swap(Task<R>& other)
    {
        // Task<R>::swap() offers the strong exception-safety guarantee as long as R
        // does for its swap, or (lacking swap specialization) for it move operations.
        swapImpl(other);
    }

    const ITaskListener<R>& listener() const _ut_noexcept
    {
        ut_dcheck(this->isValid());

        return *mListener;
    }

    ITaskListener<R>& listener() _ut_noexcept
    {
        ut_dcheck(this->isValid());

        return *mListener;
    }

    template <class T>
    const T& listenerAs() const _ut_noexcept
    {
        return static_cast<const T&>(listener()); // safe cast
    }

    template <class T>
    T& listenerAs() _ut_noexcept
    {
        return static_cast<T&>(listener()); // safe cast
    }

    bool isRunning() const _ut_noexcept
    {
        ut_assert(this->mState < ST_Running || promise()->mTask == this);

        return this->mState >= ST_RunningPromiseless;
    }

    Promise<R> takePromise() _ut_noexcept
    {
        ut_dcheck(this->isValid());

        ut_dcheck((this->mState == AwaitableBase::ST_Initial
            || this->mState == ST_RunningPromiseless) &&
            "Promise already taken");

        // Pointers get updated whenever Promise or Task is moved.
        Promise<R> promise(this);
        this->setPromise(&promise);

        return promise;
    }

    void detach() _ut_noexcept
    {
        ut_dcheck(this->isValid());
        ut_dcheck(isRunning());
        ut_dcheck(hasPromise());

        promise()->mState = Promise<R>::ST_OpRunningDetached;

        detail::CommonAwaitable<R>::reset(AwaitableBase::ST_Initial);

        if (mListener) {
            auto listener = std::move(mListener);
            mListener.reset();
            listener->onDetach();
        }
    }

    void cancel() _ut_noexcept
    {
        ut_dcheck(this->isValid());
        ut_dcheck(isRunning());

        if (hasPromise())
            promise()->mState = Promise<R>::ST_OpCanceled;

        detail::CommonAwaitable<R>::reset(detail::CommonAwaitable<R>::ST_Canceled);

        mListener.reset();
    }

private:
    Task(const Task& other) = delete;
    Task& operator=(const Task& other) = delete;

    // Async operation is under way, but the Promise has been released. Completion is possible only
    // after restoring it via takePromise().
    //
    // Releasing the Promise is useful when the completing context has direct access to the Task
    // object and doesn't need to keep the Promise around. For example, C style async APIs carry
    // only a void* for context, so instead of a Promise the callback function may have a Task* to
    // complete. For this to work the Task object must not be moved or deleted while the async
    // operation is in progress.
    static const uintptr_t ST_RunningPromiseless = AwaitableBase::ST_Initial + 1;

    // Any value larger than or equal to ST_Running is assumed to be a pointer to the Promise.
    static const uintptr_t ST_Running = ST_RunningPromiseless + 1;

    // Reserve space for two pointers (virtual table ptr + managed data).
    using erased_listener_type = ut::VirtualObjectData<
        ITaskListener<result_type>,
        virtual_header_size + ptr_size, align_of_ptr>;

    static_assert(sizeof(erased_listener_type) == virtual_header_size + ptr_size,
        "Unexpected virtual class layout");

    static_assert(std::alignment_of<erased_listener_type>::value == ptr_size,
        "Unexpected virtual class alignment");

    template <class T>
    Task(DelegateTag, Task<T>&& other)
        : detail::CommonAwaitable<R>(std::move(other))
        , mListener(std::move(other.mListener))
    {
        static_assert(std::is_same<R, T>::value,
            "May not construct from a Task of a different kind");

        // Adjust promise-task pointer.
        if (this->mState >= ST_Running) {
            ut_assert(promise()->mTask == &other);
            promise()->mTask = this;
        }
    }

    Task& assignImpl(Task&& other)
    {
        ut_assert(this != &other);

        auto *prevPromise = hasPromise() ? promise() : nullptr;

        detail::CommonAwaitable<R>::operator=(std::move(other)); // may throw

        if (prevPromise != nullptr)
            prevPromise->mState = Promise<R>::ST_OpCanceled;

        // Adjust promise-task pointer.
        if (this->mState >= ST_Running) {
            ut_assert(promise()->mTask == &other);
            promise()->mTask = this;
        }

        // Remaining fields are NoThrowMovable.
        mListener = std::move(other.mListener);

        return *this;
    }

    void swapImpl(Task& other)
    {
        detail::CommonAwaitable<R>::swap(other); // may throw

        // Adjust promise-task pointers.
        if (this->mState >= ST_Running) {
            ut_assert(promise()->mTask == &other);
            promise()->mTask = this;
        }
        if (other.mState >= ST_Running) {
            ut_assert(other.promise()->mTask == this);
            other.promise()->mTask = &other;
        }

        // Remaining fields are NoThrowMovable.
        ut::swap(mListener, other.mListener);
    }

    bool hasPromise() const _ut_noexcept
    {
        ut_assert(this->mState < ST_Running || promise()->mTask == this);

        return this->mState >= ST_Running;
    }

    const Promise<R>* promise() const _ut_noexcept
    {
        ut_assert(this->mState >= ST_Running);

        return static_cast<const Promise<R>*>(this->mStateAsPtr); // safe cast
    }

    Promise<R>* promise() _ut_noexcept
    {
        ut_assert(this->mState >= ST_Running);

        return static_cast<Promise<R>*>(this->mStateAsPtr); // safe cast
    }

    void setPromise(Promise<R> *promise) _ut_noexcept
    {
        if (promise == nullptr) {
            this->mState = ST_RunningPromiseless;
        } else {
            this->mStateAsPtr = promise;
            ut_assert(this->mState >= ST_Running);
        }
    }

    template <class ...Args>
    void complete(Args&&... args) _ut_noexcept
    {
        ut_assert(!this->isReady() && hasPromise());

        if (this->initializeResult(std::forward<Args>(args)...))
            onDone(AwaitableBase::ST_Completed);
        else
            onDone(AwaitableBase::ST_Failed);
    }

    void fail(Error error) _ut_noexcept
    {
        ut_assert(!this->isReady() && hasPromise());

        this->initializeError(std::move(error));
        onDone(AwaitableBase::ST_Failed);
    }

    void onDone(uintptr_t state) _ut_noexcept
    {
        promise()->mState = Promise<R>::ST_OpDone;
        this->mState = state;

        Awaiter *awaiter = movePtr(this->mAwaiter);

        if (mListener)
            mListener->onDone(*this);

        if (awaiter != nullptr)
            awaiter->resume(this);
    }

    erased_listener_type mListener;

    friend class Promise<R>;
    friend class PromiseMixin<R>;

    template <class Listener>
    friend struct detail::TaskListenerTraits;

    template <class L, class ...Args>
    friend auto makeTaskWithListener(Args&&... args) _ut_noexcept
        -> Task<typename detail::TaskListenerTraits<L>::result_type>;
};

static_assert(sizeof(Task<char>) ==
    sizeof(detail::CommonAwaitable<char>) + 2 * ptr_size, "");

//
// Specializations
//

template <class R, EnableIfNoThrowMovable<R> = nullptr>
void swap(Task<R>& a, Task<R>& b) _ut_noexcept
{
    a.swap(b);
}

template <class R, DisableIfNoThrowMovable<R> = nullptr>
void swap(Task<R>& a, Task<R>& b)
{
    a.swap(b);
}

//
// Promise
//

class PromiseBase
{
public:
    enum State : uintptr_t
    {
        // Invalid state: Promise has been moved into another object.
        ST_Moved,

        // Invalid state: Operation has been canceled (Task handle destructed while attached).
        ST_OpCanceled,

        // Operation has completed or failed.
        ST_OpDone,

        // Operation is running but it is no longer exposed via a Task handle.
        ST_OpRunningDetached,

        // Operation is running and it has an attached Task handle. Any value larger than
        // or equal to ST_OpRunning is assumed to be a pointer to Task.
        ST_OpRunning
    };

    State state() const _ut_noexcept
    {
        // Convert pointer range to ST_OpRunning.
        return (mState > ST_OpRunning) ? ST_OpRunning : mState;
    }

    bool isValid() const _ut_noexcept
    {
        return mState > ST_OpCanceled;
    }

    bool isCompletable() const _ut_noexcept
    {
        return mState >= ST_OpRunning;
    }

protected:
    PromiseBase() _ut_noexcept { }

    union
    {
        State mState;
        void *mTask;
    };
};

template <class R = void>
class SharedPromise;

template <class R>
class PromiseMixin;

template <class R>
class Promise
    : public PromiseBase
    , public PromiseMixin<R>
{
public:
    Promise(Promise&& other) _ut_noexcept
    {
        other.moveInto(*this);
    }

    Promise& operator=(Promise&& other) _ut_noexcept
    {
        ut_assert(this != &other);

        // A running Task gets automatically canceled if its Promise is destroyed.
        if (isCompletable()) {
            ut_assert(mTask != other.mTask);
            cancel();
        }

        other.moveInto(*this);

        return *this;
    }

    ~Promise() _ut_noexcept
    {
        // A running Task gets automatically canceled if its Promise is destroyed.
        if (isCompletable())
            cancel();
    }

    void swap(Promise<R>& other) _ut_noexcept
    {
        std::swap(mState, other.mState);

        // Adjust promise-task pointers.
        if (mState >= ST_OpRunning) {
            ut_assert(task()->promise() == &other);
            task()->setPromise(this);
        }
        if (other.mState >= ST_OpRunning) {
            ut_assert(other.task()->promise() == this);
            other.task()->setPromise(&other);
        }
    }

    SharedPromise<R> share()
    {
        return SharedPromise<R>(std::move(*this));
    }

    void release() _ut_noexcept
    {
        ut_dcheck(isCompletable());

        task()->setPromise(nullptr);
        mState = ST_Moved;
    }

    void cancel() _ut_noexcept
    {
        ut_dcheck(isCompletable());

        task()->cancel();
        ut_assert(mState == ST_OpCanceled);
    }

    void fail(Error error) _ut_noexcept
    {
        ut_dcheck(isCompletable());

        task()->fail(std::move(error));
    }

#ifdef UT_NO_EXCEPTIONS
    void fail(Error::value_type error) _ut_noexcept
    {
        fail(Error(std::move(error)));
    }
#else
    template <class E>
    void fail(E exception)
    {
        fail(makeExceptionPtr(std::move(exception)));
    }
#endif

private:
    Promise(const Promise& other) = delete;
    Promise& operator=(const Promise& other) = delete;

    explicit Promise(Task<R> *task) _ut_noexcept
    {
        mTask = task;
    }

    Task<R>* task() _ut_noexcept
    {
        ut_assert(mState >= ST_OpRunning);

        auto *task = static_cast<Task<R>*>(mTask); // safe cast
        ut_assert(task->promise() == this);

        return task;
    }

    void moveInto(Promise<R>& other) _ut_noexcept
    {
        if (isCompletable())
            task()->setPromise(&other);

        other.mState = mState;
        mState = ST_Moved;
    }


    friend class PromiseMixin<R>;
    friend class Task<R>;
};

//
// Specializations
//

template <class R>
void swap(Promise<R>& a, Promise<R>& b)
{
    a.swap(b);
}

//
// Promise result mixins
//

template <class R>
class PromiseMixin
{
public:
    template <class ...Args>
    void complete(Args&&... args) _ut_noexcept
    {
        static_assert(std::is_constructible<R, Args...>::value,
            "Invalid arguments");

        auto& thiz = static_cast<Promise<R>&>(*this); // safe cast

        thiz.task()->complete(std::forward<Args>(args)...);
    }

    template <class ...Args>
    void operator()(Args&&... args) _ut_noexcept
    {
        complete(std::forward<Args>(args)...);
    }
};

template <>
class PromiseMixin<void>
{
public:
    void complete() _ut_noexcept
    {
        auto& thiz = static_cast<Promise<void>&>(*this); // safe cast

        thiz.task()->complete();
    }

    void operator()() _ut_noexcept
    {
        complete();
    }
};

//
// SharedPromise
//

template <class R>
class SharedPromiseMixin;

template <class R>
class SharedPromise : public SharedPromiseMixin<R>
{
public:
    SharedPromise() _ut_noexcept { }

    SharedPromise(const SharedPromise& other) _ut_noexcept
        : mSharedPromise(other.mSharedPromise) { }

    SharedPromise(SharedPromise&& other) _ut_noexcept
        : mSharedPromise(std::move(other.mSharedPromise)) { }

    SharedPromise& operator=(const SharedPromise& other) _ut_noexcept
    {
        mSharedPromise = other.mSharedPromise;

        return *this;
    }

    SharedPromise& operator=(SharedPromise&& other) _ut_noexcept
    {
        ut_assert(this != &other);

        mSharedPromise = std::move(other.mSharedPromise);

        return *this;
    }

    bool isValid() const _ut_noexcept
    {
        return mSharedPromise != nullptr && mSharedPromise->isValid();
    }

    bool isCompletable() const _ut_noexcept
    {
        return mSharedPromise != nullptr && mSharedPromise->isCompletable();
    }

    void cancel() const _ut_noexcept
    {
        if (mSharedPromise != nullptr) {
            auto& promise = *mSharedPromise;

            if (promise.isCompletable())
                promise.cancel();
        }
    }

    void fail(Error error) const _ut_noexcept
    {
        if (mSharedPromise != nullptr) {
            auto& promise = *mSharedPromise;

            if (promise.isCompletable())
                promise.fail(std::move(error));
        }
    }

#ifdef UT_NO_EXCEPTIONS
    void fail(Error::value_type error) const _ut_noexcept
    {
        fail(Error(std::move(error)));
    }
#else
    template <class E>
    void fail(E exception) const
    {
        fail(makeExceptionPtr(std::move(exception)));
    }
#endif

    Promise<R>& promise() const
    {
        ut_dcheck(mSharedPromise &&
            "SharedPromise has been moved");

        return *mSharedPromise;
    }

private:
    explicit SharedPromise(Promise<R>&& promise)
        : mSharedPromise(std::make_shared<Promise<R>>(std::move(promise))) { }

    std::shared_ptr<Promise<R>> mSharedPromise;

    friend class SharedPromiseMixin<R>;
    friend class Promise<R>;
};

template <class R>
class SharedPromiseMixin
{
public:
    template <class ...Args>
    void complete(Args&&... args) const _ut_noexcept
    {
        auto& thiz = static_cast<const SharedPromise<R>&>(*this); // safe cast

        if (thiz.mSharedPromise != nullptr) {
            auto& promise = *thiz.mSharedPromise;

            if (promise.isCompletable())
                promise.complete(std::forward<Args>(args)...);
        }
    }

    template <class ...Args>
    void operator()(Args&&... args) const _ut_noexcept
    {
        complete(std::forward<Args>(args)...);
    }
};

template <>
class SharedPromiseMixin<void>
{
public:
    void complete() const _ut_noexcept
    {
        auto& thiz = static_cast<const SharedPromise<void>&>(*this); // safe cast

        if (thiz.mSharedPromise != nullptr) {
            auto& promise = *thiz.mSharedPromise;

            if (promise.isCompletable())
                promise.complete();
        }
    }

    void operator()() const _ut_noexcept
    {
        complete();
    }
};

//
// Task generators
//

template <class L, class ...Args>
auto makeTaskWithListener(Args&&... args) _ut_noexcept
    -> Task<typename detail::TaskListenerTraits<L>::result_type>
{
    using result_type = typename L::result_type;

    static_assert(std::is_nothrow_constructible<L, Args&&...>::value,
        "Constructor Listener(Args&&...) may not throw. "
        "Consider adding noexcept or throw() specifier");

    return Task<result_type>(TypeInPlaceTag<L>(), std::forward<Args>(args)...);
}

inline Task<void> makeCompletedTask() _ut_noexcept
{
    Task<void> task;
    task.takePromise().complete();
    return task;
}

template <class R, class ...Args>
Task<R> makeCompletedTask(Args&&... args)
{
    Task<R> task;
    task.takePromise().complete(R(std::forward<Args>(args)...));

#ifndef UT_NO_EXCEPTIONS
    // Throw if move/copy constructor failed.
    if (task.hasError())
        rethrowException(task.error());
#endif

    return task;
}

template <class R = void>
Task<R> makeFailedTask(Error error) _ut_noexcept
{
    Task<R> task;
    task.takePromise().fail(std::move(error));
    return task;
}

#ifdef UT_NO_EXCEPTIONS
template <class R = void>
Task<R> makeFailedTask(Error::value_type error)
{
    return makeFailedTask<R>(Error(std::move(error)));
}
#else
template <class R = void, class E>
Task<R> makeFailedTask(E exception)
{
    return makeFailedTask<R>(makeExceptionPtr(std::move(exception)));
}
#endif

template <class R = void, class T>
Task<R> makeTaskWithResource(T resource) _ut_noexcept
{
    using listener_type = detail::BoundResourceListener<R, T>;

    return makeTaskWithListener<listener_type>(std::move(resource));
}

//
// Lightweight type erasure for awaitables. The awaitable object is passed by reference and must
// remain valid until the operation completes or is canceled. Calling task.cancel() or deleting /
// overwriting the task object before completion will trigger the cancellationHandler.
//

template <class Awaitable, class CancellationHandler, class Alloc>
Task<AwaitableResult<Awaitable>> asTask(std::allocator_arg_t, const Alloc& alloc, Awaitable& awt,
    CancellationHandler cancellationHandler)
{
    return detail::asTaskImpl(std::allocator_arg, alloc, awt, std::move(cancellationHandler));
}

template <class Awaitable, class Alloc>
Task<AwaitableResult<Awaitable>> asTask(std::allocator_arg_t, const Alloc& alloc, Awaitable& awt)
{
    return detail::asTaskImpl(std::allocator_arg, alloc, awt,
        detail::IgnoreCancellation<Awaitable>());
}

template <class Awaitable, class CancellationHandler>
Task<AwaitableResult<Awaitable>> asTask(Awaitable& awt, CancellationHandler cancellationHandler)
{
    return detail::asTaskImpl(std::allocator_arg, std::allocator<char>(), awt,
        std::move(cancellationHandler));
}

template <class Awaitable>
Task<AwaitableResult<Awaitable>> asTask(Awaitable& awt)
{
    return detail::asTaskImpl(std::allocator_arg, std::allocator<char>(), awt,
        detail::IgnoreCancellation<Awaitable>());
}

//
// Shims
//

namespace detail
{
    template <class C>
    class HasTaskField
    {
        static std::true_type checkTask(AwaitableBase&);

        template <class T> static auto test(T *container)
            -> decltype(checkTask(selectAwaitable(container->task)));

        template <class T> static std::false_type test(...);

    public:
        using type = decltype(test<C>(nullptr));
        static const bool value = type::value;
    };
}

// Awaitable shims

template <class R>
Error awaitable_takeError(Task<R>& task) _ut_noexcept
{
    return std::move(task.error());
}

template <class R>
R awaitable_takeResult(Task<R>& task)
{
    return std::move(task.result());
}

// Selector shims
//

template <class T, EnableIf<detail::HasTaskField<T>::value> = nullptr>
AwaitableBase& selectAwaitable(T& item) _ut_noexcept
{
    return selectAwaitable(item.task);
}

}
