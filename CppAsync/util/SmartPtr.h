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
#include "Meta.h"
#include "PreAlloc.h"
#include <memory>

namespace ut {

//
// Extensions for unique_ptr
//

template <class T, class ...Args>
std::unique_ptr<T, std::default_delete<T>> makeUnique(Args&&... args)
{
    return std::unique_ptr<T>(new T(
        std::forward<Args>(args)...));
}

template <class T>
std::unique_ptr<T, std::default_delete<T>> asUnique(T *value) _ut_noexcept
{
    return std::unique_ptr<T>(value);
}

//
// Extensions for shared_ptr
//

#ifdef UT_NO_EXCEPTIONS

namespace detail
{
    template <class Alloc, std::size_t N>
    struct DefaultDealloc : private Alloc
    {
        DefaultDealloc(const Alloc& alloc) _ut_noexcept
            : Alloc(alloc) { }

        void operator()(void *p) _ut_noexcept
        {
            this->deallocate(static_cast<typename Alloc::value_type*>(p), N);
        }
    };
}

template <class T, class Alloc, class ...Args>
std::shared_ptr<T> allocateSharedNoThrow(const Alloc& alloc, Args&&... args) _ut_noexcept
{
    static const size_t alignment = Max<ptr_size, Max<std::alignment_of<Alloc>::value,
        std::alignment_of<T>::value>::value>::value;

    using aligned_type = AlignedStorage<alignment, alignment>;
    static_assert(sizeof(aligned_type) == alignment, "");

    using aligned_alloc_type = RebindAlloc<Alloc, aligned_type>;
    aligned_alloc_type alignedAlloc(alloc);

    // Estimate the amount of storage needed for control block plus T object. If this is
    // insufficient, PreAlloc will issue an error at compile time.

#if defined(_MSC_VER)
    static const size_t ref_counter_size = 4;
#elif defined(__GLIBCXX__) // libstdc++
    static const size_t ref_counter_size = 4;
#elif defined(_LIBCPP_VERSION) // libc++
    static const size_t ref_counter_size = sizeof(long);
#else
    static const size_t ref_counter_size = sizeof(long);
#endif

    static const size_t alloc_size = RoundUp<virtual_header_size + 2 * ref_counter_size,
        alignment>::value + RoundUp<sizeof(PreAlloc<T, detail::DefaultDealloc<aligned_alloc_type,
            0>, 0>),
        alignment>::value + RoundUp<sizeof(T), alignment>::value;

    void *p = alignedAlloc.allocate(alloc_size / alignment);

    if (p == nullptr) {
        return std::shared_ptr<T>();
    } else {
        auto preAlloc = makePreAlloc<alloc_size>(p,
            detail::DefaultDealloc<aligned_alloc_type, alloc_size / alignment>(alignedAlloc));

        // Use our infallible allocator.
        return std::allocate_shared<T>(preAlloc, std::forward<Args>(args)...);
    }
}

#endif

//
// Move semantics for regular pointers
//

template <class T>
T* movePtr(T*& value) _ut_noexcept
{
    T *moved = value;
    value = nullptr;
    return moved;
}

}
