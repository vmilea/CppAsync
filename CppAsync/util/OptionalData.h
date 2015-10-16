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
#include "TypeTraits.h"
#include <utility>

namespace ut {

template <class T>
class OptionalData
{
public:
    using value_type = T;

    OptionalData() _ut_noexcept { }

    template <class ...Args>
    explicit OptionalData(InPlaceTag, Args&&... args)
    {
        construct(std::forward<Args>(args)...);
    }

    OptionalData(const T& value)
    {
        construct(value);
    }

    OptionalData(T&& value)
    {
        construct(std::move(value));
    }

    OptionalData(bool otherHasValue, const OptionalData<T>& other)
    {
        if (otherHasValue)
            construct(other.value());
    }

    OptionalData(bool otherHasValue, OptionalData<T>&& other)
    {
        if (otherHasValue)
            construct(std::move(other.value()));
    }

    // Note: assumes there is no previous value
    template <class ...Args>
    void construct(Args&&... args)
    {
        new (&mData) T(std::forward<Args>(args)...);
    }

    // Note: assumes there is a previous value
    void destruct() _ut_noexcept
    {
        value().~T();
    }

    void reset(bool hasValue) _ut_noexcept
    {
        if (hasValue)
            destruct();
    }

    void assign(bool hasValue, const T& value)
    {
        if (hasValue)
            this->value() = value;
        else
            construct(value);
    }

    void assign(bool hasValue, T&& value)
    {
        if (hasValue)
            this->value() = std::move(value);
        else
            construct(std::move(value));
    }

    void assign(bool hasValue, bool otherHasValue, const OptionalData<T>& other)
    {
        if (hasValue) {
            if (otherHasValue)
                value() = other.value();
            else
                destruct();
        } else {
            if (otherHasValue)
                construct(other.value());
        }
    }

    void assign(bool hasValue, bool otherHasValue, OptionalData<T>&& other)
    {
        if (hasValue) {
            if (otherHasValue)
                value() = std::move(other.value());
            else
                destruct();
        } else {
            if (otherHasValue)
                construct(std::move(other.value()));
        }
    }

    void swap(bool hasValue, bool otherHasValue, OptionalData& other)
    {
        if (hasValue) {
            if (otherHasValue) {
                using std::swap;
                swap(value(), other.value());
            } else {
                other.construct(std::move(value()));
                destruct();
            }
        } else {
            if (otherHasValue) {
                construct(std::move(other.value()));
                other.destruct();
            }
        }
    }

    const T& value() const _ut_noexcept
    {
        return ptrCast<const T&>(mData); // safe cast
    }

    T& value() _ut_noexcept
    {
        return ptrCast<T&>(mData); // safe cast
    }

private:
    OptionalData(const OptionalData& other) = delete;
    OptionalData& operator=(const OptionalData& other) = delete;

    StorageFor<T> mData;
};

}
