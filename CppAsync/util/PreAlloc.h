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
#include "Cast.h"

namespace ut {

template <class T, class Dealloc, std::size_t Capacity>
class PreAlloc : private Dealloc
{
public:
    using value_type = T;

    PreAlloc(void *buf, const Dealloc& dealloc) _ut_noexcept
        : Dealloc(dealloc)
        , mBuf(buf)
    {
        ut_assert(mBuf != nullptr);
    }

    PreAlloc(const PreAlloc& other) _ut_noexcept
        : Dealloc(other)
        , mBuf(other.mBuf)
    {
        // Transfer owenership, auto_ptr style.
        other.mBuf = nullptr;
    }

    template <class U>
    PreAlloc(const PreAlloc<U, Dealloc, Capacity>& other) _ut_noexcept
        : Dealloc(other)
        , mBuf(other.mBuf)
    {
        // Transfer owenership, auto_ptr style.
        other.mBuf = nullptr;
    }

    PreAlloc& operator=(const PreAlloc& other) _ut_noexcept
    {
        if (this != &other) {
            Dealloc::operator=(other);

            // Transfer owenership, auto_ptr style.
            mBuf = other.mBuf;
            other.mBuf = nullptr;
        }

        return *this;
    }

    ~PreAlloc() _ut_noexcept
    {
        // Make sure not to leak the buffer.
        ut_assert(mBuf == nullptr);
    }

    T* allocate(std::size_t n) _ut_noexcept
    {
        static_assert(Capacity >= sizeof(T),
            "Insufficient capacity");

        ut_assert(mBuf != nullptr);
        ut_assert(n * sizeof(T) <= Capacity);

        auto *p = static_cast<T*>(mBuf);

        // Invalidate. Only a single allocate() is allowed.
        mBuf = nullptr;

        return p;
    }

    void deallocate(T* p, std::size_t n) _ut_noexcept
    {
        ut_assert(mBuf == nullptr);
        ut_assert(n * sizeof(T) <= Capacity);

        (*this)(static_cast<void*>(p)); // safe
    }

    using pointer = value_type*; // MSVC 12.0/14.0 workaround for shared_ptr

    template <class U> // MSVC 12.0 workaround for shared_ptr
    struct rebind
    {
        using other = PreAlloc<U, Dealloc, Capacity>;
    };

    void destroy(T *p) _ut_noexcept // MSVC 12.0 workaround for shared_ptr
    {
        p->~T();
    }

private:
    mutable void *mBuf;

    template <class OtherT, class OtherAlloc, std::size_t OtherCapacity>
    friend class PreAlloc;
};

template <class T, class Dealloc, std::size_t Capacity>
bool operator==(const PreAlloc<T, Dealloc, Capacity>& a,
    const PreAlloc<T, Dealloc, Capacity>& b) _ut_noexcept
{
    return &a == &b;
}

template <class T, class Dealloc, std::size_t Capacity>
bool operator!=(const PreAlloc<T, Dealloc, Capacity>& a,
    const PreAlloc<T, Dealloc, Capacity>& b) _ut_noexcept
{
    return !(a == b);
}

template <std::size_t Capacity, class T = char, class Dealloc>
auto makePreAlloc(void *buf, const Dealloc& dealloc) _ut_noexcept
    -> PreAlloc<T, Dealloc, Capacity>
{
    return PreAlloc<T, Dealloc, Capacity>(buf, dealloc);
}

}
