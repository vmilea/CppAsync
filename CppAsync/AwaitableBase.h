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
#include "util/Cast.h"
#include "util/TypeTraits.h"
#include "Awaitable.h"
#include <iterator>
#include <memory>

namespace std
{
    template <class T>
    class reference_wrapper;
}

namespace ut {

namespace detail
{
    template <class T>
    struct IsPlainAwtBaseReference : BoolConstant<
        IsPlainReference<T>::value &&
        std::is_base_of<AwaitableBase, RemoveReference<T>>::value> { };

    template <class T>
    struct IsPlainOrRvalueAwtBaseReference : BoolConstant<
        IsPlainOrRvalueReference<T>::value &&
        std::is_base_of<AwaitableBase, RemoveReference<T>>::value> { };
}

class AwaitableBase
{
public:
    bool isValid() const _ut_noexcept
    {
        return mState > ST_Invalid3;
    }

    bool isReady() const _ut_noexcept
    {
        ut_dcheck(isValid());

        return mState == ST_Completed || mState == ST_Failed;
    }

    bool hasError() const _ut_noexcept
    {
        ut_dcheck(isValid());
        ut_assert(mState != ST_Failed || castError() != Error());

        return mState == ST_Failed;
    }

    const Error& error() const _ut_noexcept
    {
        ut_dcheck(hasError());

        return castError();
    }

    Error& error() _ut_noexcept
    {
        ut_dcheck(hasError());

        return castError();
    }

    Awaiter* awaiter() const _ut_noexcept
    {
        ut_dcheck(isValid());
        ut_assert(!isReady() || mAwaiter == nullptr);

        return mAwaiter;
    }

    void setAwaiter(Awaiter *awaiter) _ut_noexcept
    {
        ut_dcheck(!isReady() &&
            "Awaiter may be set only if awaitable is not yet ready");
        ut_dcheck((awaiter == nullptr || mAwaiter == nullptr) &&
            "Awaiter should be cleared before being replaced");

        mAwaiter = awaiter;
    }

protected:
    // Custom invalid state. Meaning for CommonAwaitable<R>: object moved.
    static const uintptr_t ST_Invalid0 = 0;

    // Custom invalid state. Meaning for CommonAwaitable<R>: operation canceled.
    static const uintptr_t ST_Invalid1 = 1;

    // Custom invalid state
    static const uintptr_t ST_Invalid2 = 2;

    // Custom invalid state
    static const uintptr_t ST_Invalid3 = 3;

    // Awaitable is ready with a result.
    static const uintptr_t ST_Completed = 4;

    // Awaitable is ready with an error.
    static const uintptr_t ST_Failed = 5;

    // Default state
    static const uintptr_t ST_Initial = 6;

    // ... other custom states

#ifdef UT_DISABLE_EXCEPTIONS
    AwaitableBase() _ut_noexcept
        : mAwaiter(nullptr)
        , mState(ST_Initial)
        , mError(Error()) { }
#else
    AwaitableBase() _ut_noexcept
        : mAwaiter(nullptr)
        , mState(ST_Initial) { }
#endif

    const Error& castError() const _ut_noexcept
    {
        return *ptrCast<const Error*>(
            ptrCast<const char*>(this) + 2 * ptr_size);
    }

    Error& castError() _ut_noexcept
    {
        return *ptrCast<Error*>(
            ptrCast<char*>(this) + 2 * ptr_size);
    }

    Awaiter *mAwaiter;
    union { uintptr_t mState; void *mStateAsPtr; };
};

static_assert(sizeof(AwaitableBase) % max_align_size == 0,
    "Required for safe access to error field regardless of the alignment of derived class.");

//
// Specializations
//

inline Error awaitable_takeError(AwaitableBase& awt) _ut_noexcept
{
    return std::move(awt.error());
}

//
// Selector shims
//

// Getting an error here means you are trying to use awaitAll/Any/Some on a collection of
// unknown types. You can teach the library how to probe new types and access the underlying
// awaitable by overloading selectAwaitable().

inline AwaitableBase& selectAwaitable(AwaitableBase& awt) _ut_noexcept
{
    return awt;
}

template <class T>
AwaitableBase& selectAwaitable(T *item) _ut_noexcept
{
    return selectAwaitable(*item);
}

template <class T, class Deleter>
AwaitableBase& selectAwaitable(const std::unique_ptr<T, Deleter>& item) _ut_noexcept
{
    return selectAwaitable(*item);
}

template <class T>
AwaitableBase& selectAwaitable(const std::reference_wrapper<T>& item) _ut_noexcept
{
    return selectAwaitable(item.get());
}

}
