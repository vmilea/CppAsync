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
#include "Cast.h"
#include "Meta.h"
#include <utility>

namespace ut {

template <class T>
class Optional
{
public:
    using value_type = T;

    Optional() _ut_noexcept
        : mHasValue(false) { }

    Optional(const T& value)
        : mHasValue(true)
    {
        new(&mData) T(value);
    }

    Optional(T&& value)
        : mHasValue(true)
    {
        new(&mData) T(std::move(value));
    }

    template <class ...Args>
    Optional(InPlaceTag, Args&&... args)
    {
        new (&mData) T(std::forward<Args>(args)...);
    }

    Optional(const Optional& other)
        : mHasValue(other.mHasValue)
    {
        if (mHasValue)
            new(&mData) T(other.get());
    }

    // Note: moved from object will still contain a value
    Optional(Optional&& other)
        : mHasValue(other.mHasValue)
    {
        if (mHasValue)
            new(&mData) T(std::move(other.get()));
    }

    template<class U>
    explicit Optional(const Optional<U>& other)
        : mHasValue(other.mHasValue)
    {
        if (mHasValue)
            new(&mData) T(other.get());
    }

    // Note: moved from object will still contain a value
    template<class U>
    explicit Optional(Optional<U>&& other)
        : mHasValue(other.mHasValue)
    {
        if (mHasValue)
            new(&mData) T(std::move(other.get()));
    }

    ~Optional()
    {
        if (mHasValue)
            get().~T();
    }

    Optional& operator=(const Optional& other)
    {
        return assign(other);
    }

    Optional& operator=(Optional&& other)
    {
        return assign(std::move(other));
    }

    template <class U>
    Optional& operator=(const Optional<U>& other)
    {
        return assign(other);
    }

    // Note: moved from object will still contain a value
    template <class U>
    Optional& operator=(Optional<U>&& other)
    {
        return assign(std::move(other));
    }

    // Note: remains without a value if constructor throws
    template <class ...Args>
    Optional& emplace(Args&&... args)
    {
        reset();

        new(&mData) T(std::forward<Args>(args)...);
        mHasValue = true;

        return *this;
    }

    void swap(Optional& other)
    {
        if (mHasValue) {
            if (other.mHasValue) {
                using std::swap;
                swap(get(), other.get());
            } else {
                new(&other.mData) T(std::move(get()));
                get().~T();
                other.mHasValue = true;
                mHasValue = false;
            }
        } else {
            if (other.mHasValue) {
                new(&mData) T(std::move(other.get()));
                other.get().~T();
                mHasValue = true;
                other.mHasValue = false;
            }
        }
    }

    void reset() _ut_noexcept
    {
        if (mHasValue) {
            get().~T();
            mHasValue = false;
        }
    }

    const T* operator->() const _ut_noexcept
    {
        return &get();
    }

    T* operator->() _ut_noexcept
    {
        return &get();
    }

    const T& operator*() const _ut_noexcept
    {
        return get();
    }

    T& operator*() _ut_noexcept
    {
        return get();
    }

    const T& value() const _ut_noexcept
    {
        return get();
    }

    T& value() _ut_noexcept
    {
        return get();
    }

    template <class U>
    T valueOr(U&& value) const
    {
        if (mHasValue)
            return get();
        else
            return T(std::forward<U>(value));
    }

    explicit operator bool() const _ut_noexcept
    {
        return mHasValue;
    }

private:
    const T& get() const _ut_noexcept
    {
        ut_dcheck(mHasValue &&
            "Optional value not set");

        return ptrCast<const T&>(mData); // safe cast
    }

    T& get() _ut_noexcept
    {
        ut_dcheck(mHasValue &&
            "Optional value not set");

        return ptrCast<T&>(mData); // safe cast
    }

    template <class U>
    Optional& assign(const Optional<U>& other)
    {
        if (mHasValue) {
            if (other.mHasValue) {
                get() = other.get();
            } else {
                get().~T();
                mHasValue = false;
            }
        } else {
            if (other.mHasValue) {
                new(&mData) T(other.get());
                mHasValue = true;
            }
        }

        return *this;
    }

    template <class U>
    Optional& assign(Optional<U>&& other)
    {
        if (mHasValue) {
            if (other.mHasValue) {
                get() = std::move(other.get());
            } else {
                get().~T();
                mHasValue = false;
            }
        } else {
            if (other.mHasValue) {
                new(&mData) T(std::move(other.get()));
                mHasValue = true;
            }
        }

        return *this;
    }

    ut::StorageFor<T> mData;
    bool mHasValue;

    template <class OtherT>
    friend class Optional;

    template <class OtherT>
    friend bool operator==(const Optional<OtherT>& lhs, const Optional<OtherT>& rhs) _ut_noexcept;

    template <class OtherT>
    friend bool operator<(const Optional<OtherT>& lhs, const Optional<OtherT>& rhs) _ut_noexcept;
};

//
// Specializations
//

template <class T>
bool operator==(const Optional<T>& lhs, const Optional<T>& rhs) _ut_noexcept
{
    if (lhs)
        return rhs ? (lhs.get() == rhs.get()) : false;
    else
        return !rhs;
}

template <class T>
bool operator!=(const Optional<T>& lhs, const Optional<T>& rhs) _ut_noexcept
{
    return !(lhs == rhs);
}

template <class T>
bool operator<(const Optional<T>& lhs, const Optional<T>& rhs) _ut_noexcept
{
    if (lhs)
        return rhs ? (lhs.get() < rhs.get()) : false;
    else
        return rhs;
}

template <class T>
bool operator>(const Optional<T>& lhs, const Optional<T>& rhs) _ut_noexcept
{
    return rhs < lhs;
}

template <class T>
bool operator<=(const Optional<T>& lhs, const Optional<T>& rhs) _ut_noexcept
{
    return !(lhs > rhs);
}

template <class T>
bool operator>=(const Optional<T>& lhs, const Optional<T>& rhs) _ut_noexcept
{
    return !(lhs < rhs);
}

template <class T>
void swap(Optional<T>& lhs, Optional<T>& rhs)
{
    lhs.swap(rhs);
}

//
// Generators
//

template <class T>
Optional<Unqualified<T>> makeOptional(T&& value)
{
    return Optional<Unqualified<T>>(std::forward<T>(value));
}

}
