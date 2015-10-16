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

#include "../impl/Common.h"
#include "../impl/Assert.h"
#include "Meta.h"

namespace ut {

template <typename T>
class UncheckedMoveOnCopy
{
public:
    UncheckedMoveOnCopy(T&& value)
        : mValue(std::move(value)) { }

    UncheckedMoveOnCopy(const UncheckedMoveOnCopy& other)
        : mValue(std::move(const_cast<UncheckedMoveOnCopy&>(other).mValue)) { }

    UncheckedMoveOnCopy(UncheckedMoveOnCopy&& other)
        : mValue(std::move(other.mValue)) { }

    UncheckedMoveOnCopy& operator=(const UncheckedMoveOnCopy& other)
    {
        if (this != &other) {
            auto& mutableOther = const_cast<UncheckedMoveOnCopy&>(other);
            mValue = std::move(mutableOther.mValue);
        }

        return *this;
    }

    UncheckedMoveOnCopy& operator=(UncheckedMoveOnCopy&& other)
    {
        ut_assert(this != &other);

        mValue = std::move(other.mValue);

        return *this;
    }

    T& get() const _ut_noexcept
    {
        return const_cast<UncheckedMoveOnCopy*>(this)->mValue;
    }

    T take() const
    {
        return std::move(get());
    }

    T* operator->() const _ut_noexcept
    {
        return &get();
    }

    T& operator*() const _ut_noexcept
    {
        return get();
    }

private:
    T mValue;
};

template <typename T>
class CheckedMoveOnCopy
{
public:
    CheckedMoveOnCopy(T&& value)
        : mValue(std::move(value))
        , mIsNil(true) { }

    CheckedMoveOnCopy(const CheckedMoveOnCopy& other)
        : mValue(std::move(const_cast<CheckedMoveOnCopy&>(other).mValue))
        , mIsNil(true)
    {
        ut_checkf(other.mIsNil, "Illegal move-on-copy construction - source already moved");

        auto& mutableOther = const_cast<CheckedMoveOnCopy&>(other);
        mutableOther.mIsNil = false;
    }

    CheckedMoveOnCopy(CheckedMoveOnCopy&& other)
        : mValue(std::move(other.mValue))
        , mIsNil(other.mIsNil)
    {
        other.mIsNil = false;
    }

    CheckedMoveOnCopy& operator=(const CheckedMoveOnCopy& other)
    {
        if (this != &other) {
            ut_checkf(other.mIsNil, "Illegal move-on-copy assignment - source already moved");

            auto& mutableOther = const_cast<CheckedMoveOnCopy&>(other);
            mValue = std::move(mutableOther.mValue);
            mIsNil = true;
            mutableOther.mIsNil = false;
        }

        return *this;
    }

    CheckedMoveOnCopy& operator=(CheckedMoveOnCopy&& other)
    {
        ut_assert(this != &other);

        mValue = std::move(other.mValue);
        mIsNil = other.mIsNil;
        other.mIsNil = false;

        return *this;
    }

    T& get() const _ut_noexcept
    {
        ut_checkf(mIsNil, "Illegal access - value has been moved");

        auto& thiz = *const_cast<CheckedMoveOnCopy*>(this);

        return thiz.mValue;
    }

    T take() const
    {
        ut_checkf(mIsNil, "Illegal access - value has been moved");

        auto& thiz = *const_cast<CheckedMoveOnCopy*>(this);
        thiz.mIsNil = false;

        return std::move(thiz.mValue);
    }

    T* operator->() const _ut_noexcept
    {
        return &get();
    }

    T& operator*() const _ut_noexcept
    {
        return get();
    }

private:
    T mValue;
    bool mIsNil;
}; 

template <typename T>
UncheckedMoveOnCopy<Unqualified<T>> makeUncheckedMoveOnCopy(T&& value)
{
    static_assert(std::is_rvalue_reference<T&&>::value,
        "Expected an rvalue");

    return UncheckedMoveOnCopy<RemoveReference<T>>(std::move(value));
}

template <typename T>
CheckedMoveOnCopy<Unqualified<T>> makeCheckedMoveOnCopy(T&& value)
{
    static_assert(std::is_rvalue_reference<T&&>::value,
        "Expected an rvalue");

    return CheckedMoveOnCopy<RemoveReference<T>>(std::move(value));
}

#ifdef NDEBUG

template <typename T>
UncheckedMoveOnCopy<Unqualified<T>> makeMoveOnCopy(T&& value)
{
    return makeUncheckedMoveOnCopy(std::forward<T>(value));
}

#else

template <typename T>
CheckedMoveOnCopy<Unqualified<T>> makeMoveOnCopy(T&& value)
{
    return makeCheckedMoveOnCopy(std::forward<T>(value));
}

#endif // NDEBUG

}
