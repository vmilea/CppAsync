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
#include "Cast.h"
#include "TypeTraits.h"

namespace ut {

// Supports late initialization of wrapped object. Useful when unable to immediately
// construct some class member.
//
template <class T>
class Instance
{
public:
    Instance() _ut_noexcept { }

    ~Instance() _ut_noexcept
    {
        get()->~T();
    }

    Instance(const Instance& other)
    {
        initialize(*other);
    }

    Instance(Instance&& other)
    {
        initialize(std::move(*other));
    }

    Instance& operator=(const Instance& other)
    {
        **this = *other;

        return *this;
    }

    Instance& operator=(Instance&& other)
    {
        **this = std::move(*other);

        return *this;
    }

    template <class ...Args>
    void initialize(Args&&... args)
    {
        new (&mData) T(std::forward<Args>(args)...);
    }

    const T* get() const _ut_noexcept
    {
        return ptrCast<const T*>(&mData); // safe cast
    }

    T* get() _ut_noexcept
    {
        return ptrCast<T*>(&mData); // safe cast
    }

    const T* operator->() const _ut_noexcept
    {
        return get();
    }

    T* operator->() _ut_noexcept
    {
        return get();
    }

    const T& operator*() const _ut_noexcept
    {
        return *get();
    }

    T& operator*() _ut_noexcept
    {
        return *get();
    }

private:
    StorageFor<T> mData;
};

}
