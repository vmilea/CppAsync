/*
* Copyright 2015-2016 Valentin Milea
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

#include "../impl/Common.h"
#include "UnsafeEitherData.h"

namespace ut {

template <class A, class B>
class EitherData
{
public:
    static_assert(IsNoThrowMovable<A>::value,
        "Type A must be no-throw move-constructible and move-assignable "
        "for strong exception-safety guarantee");

    using a_type = A;
    using b_type = B;

    template <EnableIf<std::is_default_constructible<A>::value> = nullptr>
    EitherData() { }

    template <class ...Args>
    explicit EitherData(ATag /* target */, Args&&... args)
        : mData(ATag(), std::forward<Args>(args)...) { }

    template <class ...Args>
    explicit EitherData(BTag /* target */, Args&&... args)
        : mData(BTag(), std::forward<Args>(args)...) { }

    EitherData(const A& value)
        : mData(value) { }

    EitherData(A&& value) _ut_noexcept
        : mData(std::move(value)) { }

    EitherData(const B& value)
        : mData(value) { }

    EitherData(B&& value)
        : mData(std::move(value)) { }

    EitherData(bool isOtherB, const EitherData<A, B>& other)
        : mData(isOtherB, other.mData) { }

    EitherData(bool isOtherB, EitherData<A, B>&& other)
        : mData(isOtherB, std::move(other.mData)) { }

    const UnsafeEitherData<A, B>& raw() const _ut_noexcept
    {
        return mData;
    }

    UnsafeEitherData<A, B>& raw() _ut_noexcept
    {
        return mData;
    }

    void destruct(bool isB) _ut_noexcept
    {
        mData.destruct(isB);
    }

    void assign(bool isB, const A& value)
    {
        mData.assign(isB, A(value)); // copy constructor may throw
    }

    void assign(bool isB, A&& value) _ut_noexcept
    {
        mData.assign(isB, std::move(value)); // no throw
    }

    void assign(bool isB, const B& value)
    {
        assignBImpl(TagByNoThrowCopyable<B>(), isB, value);
    }

    void assign(bool isB, B&& value)
    {
        assignBImpl(TagByNoThrowMovable<B>(), isB, std::move(value));
    }

    void assign(bool isB, bool isOtherB,
        const EitherData<A, B>& other)
    {
        if (isOtherB)
            assign(isB, other.b());
        else
            assign(isB, other.a());
    }

    void assign(bool isB, bool isOtherB, EitherData<A, B>&& other)
    {
        if (isOtherB)
            assign(isB, std::move(other.b()));
        else
            assign(isB, std::move(other.a()));
    }

    void swap(bool isB, bool isOtherB, EitherData<A, B>& other)
    {
        swapImpl(TagByNoThrowMovable<B>(), isB, isOtherB, other);
    }

    const A& a() const _ut_noexcept
    {
        return mData.a();
    }

    A& a() _ut_noexcept
    {
        return mData.a();
    }

    const B& b() const _ut_noexcept
    {
        return mData.b();
    }

    B& b() _ut_noexcept
    {
        return mData.b();
    }

private:
    EitherData(const EitherData& other) = delete;
    EitherData& operator=(const EitherData& other) = delete;

    template <class U>
    void assignBImpl(NoThrowTag, bool isB, U&& value) _ut_noexcept
    {
        mData.assign(isB, std::forward<U>(value));
    }

#ifndef UT_NO_EXCEPTIONS
    template <class U>
    void assignBImpl(ThrowTag, bool isB, U&& value)
    {
        if (isB) {
            mData.assignIntoB(std::forward<U>(value)); // may throw
        } else {
            A tmp(std::move(a())); // no throw
            try {
                mData.assignIntoA(std::forward<U>(value));
            } catch (...) {
                // B constructor has thrown, rollback and rethrow
                mData.assignIntoBlank(std::move(tmp));
                throw;
            }
        }
    }
#endif

    void swapImpl(NoThrowTag, bool isB, bool isOtherB, EitherData<A, B>& other) _ut_noexcept
    {
        mData.swap(isB, isOtherB, other.mData); // no throw
    }

#ifndef UT_NO_EXCEPTIONS
    void swapImpl(ThrowTag, bool isB, bool isOtherB, EitherData<A, B>& other)
    {
        if (isOtherB) {
            if (isB) {
                mData.swapBIntoB(other.mData); // may throw
            } else {
                A tmp(std::move(a())); // no throw
                try {
                    mData.assignIntoA(std::move(other.mData.b())); // may throw
                    other.mData.assignIntoB(std::move(tmp)); // no throw
                } catch (...) {
                    // B move-constructor has thrown, rollback and rethrow.
                    mData.assignIntoBlank(std::move(tmp)); // no throw
                    throw;
                }
            }
        } else {
            if (isB) {
                A tmp(std::move(other.a())); // no throw
                try {
                    other.mData.assignIntoA(std::move(mData.b())); // may throw
                    mData.assignIntoB(std::move(tmp)); // no throw
                } catch (...) {
                    // B move-constructor has thrown, rollback and rethrow.
                    other.mData.assignIntoBlank(std::move(tmp));
                    throw;
                }
            } else {
                mData.swapAIntoA(other.mData); // no throw
            }
        }
    }
#endif

    UnsafeEitherData<A, B> mData;
};

}
