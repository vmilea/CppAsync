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
#include "../AwaitableBase.h"
#include "../util/Cast.h"
#include "../util/EitherData.h"
#include "../util/Meta.h"
#include "../util/SmartPtr.h"

namespace ut {

namespace detail
{
    template <class R>
    class CommonAwaitable : public AwaitableBase
    {
        static_assert(std::alignment_of<R>::value <= 2 * ptr_size,
            "Excessive alignment required for result type");

    public:
        using result_type = R;

        CommonAwaitable(CommonAwaitable&& other)
            : mResult(other.hasResult(), std::move(other.mResult)) // may throw
        {
            other.mResult.assign(other.hasResult(), Error());

            mAwaiter = movePtr(other.mAwaiter);

            mState = other.mState;
            other.mState = ST_Moved;
        }

        CommonAwaitable& operator=(CommonAwaitable&& other)
        {
            ut_assert(this != &other);

            if (other.hasResult()) {
                mResult.assign(hasResult(), std::move(other.mResult.b())); // may throw
                other.mResult.raw().assignAIntoB(Error());
            } else {
                mResult.assign(hasResult(), std::move(other.mResult.a()));
            }

            mAwaiter = movePtr(other.mAwaiter);

            mState = other.mState;
            other.mState = ST_Moved;

            return *this;
        }

        ~CommonAwaitable() _ut_noexcept
        {
            mResult.destruct(hasResult());
        }

        const R& result() const _ut_noexcept
        {
            ut_dcheck(isReady() && !hasError());

            return mResult.b();
        }

        R& result() _ut_noexcept
        {
            ut_dcheck(isReady() && !hasError());

            return mResult.b();
        }

        const R& get() const
        {
#ifdef UT_NO_EXCEPTIONS
            ut_dcheck(!hasError());
#else
            if (hasError())
                rethrowException(error());
#endif

            return result();
        }

        R& get()
        {
#ifdef UT_NO_EXCEPTIONS
            ut_dcheck(!hasError());
#else
            if (hasError())
                rethrowException(error());
#endif

            return result();
        }

    protected:
        // Awaitable has been moved into another object.
        static const uintptr_t ST_Moved = ST_Invalid0;

        // Underlying operation has been canceled.
        static const uintptr_t ST_Canceled = ST_Invalid1;

        CommonAwaitable() _ut_noexcept
        {
            ut_assert(static_cast<void*>(&castError()) // safe
                == static_cast<void*>(&mResult.a())); // safe

            ut_assert(static_cast<void*>(&mResult) // safe
                == static_cast<void*>(&mResult.a())); // safe
        }

        void swap(CommonAwaitable& other)
        {
            // swap() offers the strong exception-safety guarantee as long as R does
            // for its swap, or (lacking swap specialization) for it move operations.

            mResult.swap(hasResult(), other.hasResult(), other.mResult); // may throw

            // Remaining fields are NoThrowMovable
            std::swap(mAwaiter, other.mAwaiter);
            std::swap(mState, other.mState);
        }

        void reset(uintptr_t state) _ut_noexcept
        {
            mResult.assign(hasResult(), Error());

            mAwaiter = nullptr;
            mState = state;
        }

        template <class ...Args>
        bool initializeResult(Args&&... args) _ut_noexcept
        {
            return initializeResultImpl(
                TagByNoThrow<std::is_nothrow_constructible<R, Args...>::value>(),
                std::forward<Args>(args)...);
        }

        void initializeError(Error&& error) _ut_noexcept
        {
            ut_assert(!isReady());
            ut_assert(isNil(mResult.a()));
            ut_assert(!isNil(error));

            mResult.a() = std::move(error);
        }

    private:
        bool hasResult() const _ut_noexcept
        {
            return mState == ST_Completed;
        }

        template <class ...Args>
        bool initializeResultImpl(NoThrowTag, Args&&... args) _ut_noexcept
        {
            ut_assert(!isReady());
            ut_assert(isNil(mResult.a()));

            mResult.raw().emplaceBIntoA(std::forward<Args>(args)...);
            return true;
        }

#ifndef UT_NO_EXCEPTIONS
        template <class ...Args>
        bool initializeResultImpl(ThrowTag, Args&&... args) _ut_noexcept
        {
            ut_assert(!isReady());
            ut_assert(isNil(mResult.a()));

            try {
                mResult.raw().emplaceBIntoA(std::forward<Args>(args)...);
                return true;
            } catch (...) {
                mResult.raw().assignIntoBlank(currentException());
                return false;
            }
        }
#endif

        EitherData<Error, R> mResult;
    };

    //
    // CommonAwaitable<void>
    //

    template <>
    class CommonAwaitable<void> : public AwaitableBase
    {
    public:
        using result_type = void;

        CommonAwaitable(CommonAwaitable&& other) _ut_noexcept
            : mError(std::move(other.mError))
        {
            mAwaiter = movePtr(other.mAwaiter);

            mState = other.mState;
            other.mState = ST_Moved;
        }

        CommonAwaitable& operator=(CommonAwaitable&& other) _ut_noexcept
        {
            ut_assert(this != &other);

            mError = std::move(other.mError);

            mAwaiter = movePtr(other.mAwaiter);

            mState = other.mState;
            other.mState = ST_Moved;

            return *this;
        }

        ~CommonAwaitable() _ut_noexcept { }

        void get() const
        {
#ifdef UT_NO_EXCEPTIONS
            ut_dcheck(!hasError());
#else
            if (hasError())
                rethrowException(error());
#endif
        }

    protected:
        // Awaitable has been moved into another object.
        static const uintptr_t ST_Moved = ST_Invalid0;

        // Underlying operation has been canceled.
        static const uintptr_t ST_Canceled = ST_Invalid1;

        CommonAwaitable() _ut_noexcept
            : mError(Error())
        {
            ut_assert(static_cast<void*>(&castError()) // safe
                == static_cast<void*>(&mError)); // safe
        }

        void swap(CommonAwaitable& other) _ut_noexcept
        {
            std::swap(mAwaiter, other.mAwaiter);
            std::swap(mState, other.mState);
        }

        void reset(uintptr_t state) _ut_noexcept
        {
            mAwaiter = nullptr;
            mState = state;
        }

        bool initializeResult() _ut_noexcept
        {
            ut_assert(!isReady());
            ut_assert(isNil(mError));

            // Does nothing, always sucessful.
            return true;
        }

        void initializeError(Error&& error) _ut_noexcept
        {
            ut_assert(!isReady());
            ut_assert(isNil(mError));
            ut_assert(!isNil(error));

            mError = std::move(error);
        }

    private:
        Error mError;
    };

    static_assert(
        sizeof(CommonAwaitable<void>) == 2 * ptr_size +
        RoundUp<sizeof(Error), ptr_size>::value,
        "Unexpected layout for CommonAwaitable<void>");
}

}
