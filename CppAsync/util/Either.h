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
#include "../impl/Assert.h"
#include "EitherData.h"
#include "Meta.h"
#include <utility>

namespace ut {

template <class A, class B>
class Either
{
public:
    using a_type = A;
    using b_type = B;

    template <ut::EnableIf<std::is_default_constructible<A>::value> = nullptr>
    Either() { }

    template <class ...Args>
    explicit Either(ATag /* target */, Args&&... args)
        : mData(ATag(), std::forward<Args>(args)...)
        , mIsB(false) { }

    template <class ...Args>
    explicit Either(BTag /* target */, Args&&... args)
        : mData(BTag(), std::forward<Args>(args)...)
        , mIsB(true) { }

    Either(const A& value)
        : mData(value)
        , mIsB(false) { }

    Either(A&& value)
        : mData(std::move(value))
        , mIsB(false) { }

    Either(const B& value)
        : mData(value)
        , mIsB(true) { }

    Either(B&& value)
        : mData(std::move(value))
        , mIsB(true) { }

    Either(const Either<A, B>& other)
        : mData(other.isB(), other.mData)
        , mIsB(other.isB()) { }

    Either(Either<A, B>&& other)
        : mData(other.isB(), std::move(other.mData))
        , mIsB(other.isB()) { }

    ~Either()
    {
        mData.destruct(isB());
    }

    Either& operator=(const A& value)
    {
        mData.assign(isB(), value);
        mIsB = false;

        return *this;
    }

    Either& operator=(A&& value)
    {
        mData.assign(isB(), std::move(value));
        mIsB = false;

        return *this;
    }

    Either& operator=(const B& value)
    {
        mData.assign(isB(), value);
        mIsB = true;

        return *this;
    }

    Either& operator=(B&& value)
    {
        mData.assign(isB(), std::move(value));
        mIsB = true;

        return *this;
    }

    Either& operator=(const Either<A, B>& other)
    {
        mData.assign(isB(), other.isB(), other.mData);
        mIsB = other.isB();

        return *this;
    }

    Either& operator=(Either<A, B>&& other)
    {
        mData.assign(isB(), other.isB(), std::move(other.mData));
        mIsB = other.isB();

        return *this;
    }

    template <class ...Args>
    Either& emplaceA(Args&&... args)
    {
        mData.emplaceA(isB(), std::forward<Args>(args)...);
        mIsB = false;

        return *this;
    }

    template <class ...Args>
    Either& emplaceB(Args&&... args)
    {
        mData.emplaceB(isB(), std::forward<Args>(args)...);
        mIsB = true;

        return *this;
    }

    void swap(Either& other)
    {
        mData.swap(isB(), other.isB(), other.mData);
        std::swap(mIsB, other.mIsB);
    }

    bool isA() const
    {
        return !isB();
    }

    bool isB() const
    {
        return mIsB;
    }

    int which() const
    {
        return isB() ? 1 : 0;
    }

    const A& a() const
    {
        ut_dcheck(!isB() &&
            "Either<A, B> must hold a value of type A");

        return mData.a();
    }

    A& a()
    {
        ut_dcheck(!isB() &&
            "Either<A, B> must hold a value of type A");

        return mData.a();
    }

    const B& b() const
    {
        ut_dcheck(isB() &&
            "Either<A, B> must hold a value of type B");

        return mData.b();
    }

    B& b()
    {
        ut_dcheck(isB() &&
            "Either<A, B> must hold a value of type B");

        return mData.b();
    }

private:
    EitherData<A, B> mData;
    bool mIsB;
};

template <class A, class B>
void swap(Either<A, B>& lhs, Either<A, B>& rhs)
{
    lhs.swap(rhs);
}

template <class A, class B>
bool operator==(const Either<A, B>& lhs, const Either<A, B>& rhs)
{
    if (lhs.isB())
        return rhs.isB() && (lhs.b() == rhs.b());
    else
        return !rhs.isB() && (lhs.a() == rhs.a());
}

template <class A, class B>
bool operator!=(const Either<A, B>& lhs, const Either<A, B>& rhs)
{
    return !(lhs == rhs);
}

template <typename U, class A, class B,
    ut::EnableIf<std::is_convertible<U&&, A>::value> = nullptr>
bool operator==(const Either<A, B>& lhs, const U& rhs)
{
    return !lhs.isB() && lhs.a() == rhs;
}

template <typename U, class A, class B,
    ut::EnableIf<std::is_convertible<U&&, B>::value> = nullptr>
bool operator==(const Either<A, B>& lhs, const U& rhs)
{
    return lhs.isB() && lhs.b() == rhs;
}

template <typename U, class A, class B>
bool operator==(const U& lhs, const Either<A, B>& rhs)
{
    return rhs == lhs;
}

template <typename U, class A, class B>
bool operator!=(const Either<A, B>& lhs, const U& rhs)
{
    return !(lhs == rhs);
}

template <typename U, class A, class B>
bool operator!=(const U& lhs, const Either<A, B>& rhs)
{
    return !(rhs == lhs);
}

template <class A, class B>
bool operator<(const Either<A, B>& lhs, const Either<A, B>& rhs)
{
    if (lhs.isB())
        return rhs.isB() && (lhs.b() < rhs.b());
    else
        return rhs.isB() || (lhs.a() < rhs.a());
}

template <class A, class B>
bool operator>(const Either<A, B>& lhs, const Either<A, B>& rhs)
{
    return rhs < lhs;
}

template <class A, class B>
bool operator<=(const Either<A, B>& lhs, const Either<A, B>& rhs)
{
    return !(rhs < lhs);
}

template <class A, class B>
bool operator>=(const Either<A, B>& lhs, const Either<A, B>& rhs)
{
    return !(lhs < rhs);
}

}
