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
#include <cstdint>
#include <new>

namespace ut {

//
// ArenaAlloc
//

template <class T, class Arena>
class ArenaAlloc;

template <class T, class Arena>
bool operator==(const ArenaAlloc<T, Arena>& a, const ArenaAlloc<T, Arena>& b) _ut_noexcept;

template <class T, class Arena>
class ArenaAlloc
{
public:
    using value_type = T;
    using arena_type = Arena;

    ArenaAlloc(Arena& arena) _ut_noexcept
        : mArena(&arena) { }

    ArenaAlloc(const ArenaAlloc& other) = default;

    template <class U>
    ArenaAlloc(const ArenaAlloc<U, Arena>& other) _ut_noexcept
        : mArena(other.mArena) { }

    const Arena& arena() const _ut_noexcept
    {
        return *mArena;
    }

    Arena& arena() _ut_noexcept
    {
        return *mArena;
    }

    T* allocate(size_t n)
    {
        return mArena->template allocate<T>(n);
    }

    void deallocate(T* p, size_t n) _ut_noexcept
    {
        mArena->template deallocate<T>(p, n);
    }

    template <class U> // MSVC 12.0 workaround for shared_ptr
    struct rebind
    {
        using other = ArenaAlloc<U, Arena>;
    };

    void destroy(T *p) _ut_noexcept // MSVC 12.0 workaround for shared_ptr
    {
        p->~T();
    }

private:
    Arena *mArena;

    template <class OtherT, class OtherArena>
    friend class ArenaAlloc;

    friend bool operator==<T, Arena>(
        const ArenaAlloc<T, Arena>& a, const ArenaAlloc<T, Arena>& b) _ut_noexcept;
};

template <class T, class Arena>
bool operator==(const ArenaAlloc<T, Arena>& a, const ArenaAlloc<T, Arena>& b) _ut_noexcept
{
    return a.mArena == b.mArena;
}

template <class T, class Arena>
bool operator!=(const ArenaAlloc<T, Arena>& a, const ArenaAlloc<T, Arena>& b) _ut_noexcept
{
    return !(a == b);
}

template <class T = char, class Arena>
ArenaAlloc<T, Arena> makeArenaAlloc(Arena& arena) _ut_noexcept
{
    return ArenaAlloc<T, Arena>(arena);
}

//
// BufferArena
//

class BufferArenaBase
{
public:
    BufferArenaBase(char *buf, size_t capacity) _ut_noexcept
        : mBuf(buf)
        , mPos(buf)
        , mCapacity(capacity)
    {
        if (!(capacity > max_align_size && (capacity % max_align_size == 0))) {
            ut_dcheckf(false, "Capacity should be a multiple of max alignment (%d)",
                static_cast<int>(max_align_size)); // safe cast
        }
    }

    ~BufferArenaBase()
    {
        mPos = nullptr;
    }

    size_t capacity() _ut_noexcept
    {
        return mCapacity;
    }

    size_t used() _ut_noexcept
    {
        return static_cast<size_t>(mPos - mBuf); // safe cast
    }

private:
    BufferArenaBase(const BufferArenaBase& other) = delete;
    BufferArenaBase& operator=(const BufferArenaBase& other) = delete;

protected:
    const char* end() const _ut_noexcept
    {
        return mBuf + mCapacity;
    }

    bool isValidPos(const char *pos) const _ut_noexcept
    {
        return mBuf <= pos && pos <= end();
    }

    char *mBuf, *mPos;
    size_t mCapacity;
};

class LinearBufferArena : public BufferArenaBase
{
public:
    LinearBufferArena(char *buf, size_t capacity) _ut_noexcept
        : BufferArenaBase(buf, capacity) { }

    template <class T>
    T* allocate(size_t n)
    {
        return static_cast<T*>(allocateImpl(n * sizeof(T))); // safe cast
    }

    template <class T>
    void deallocate(T* p, size_t n)
    {
        deallocateImpl(p, n * sizeof(T));
    }

private:
    void* allocateImpl(size_t size)
    {
        ut_dcheck(isValidPos(mPos) &&
            "ArenaAlloc has outlived arena");

        size_t chunkSize = (size + max_align_size - 1) / max_align_size * max_align_size;

        if (mPos + chunkSize > end()) {
#ifdef UT_NO_EXCEPTIONS
            return nullptr;
#else
            throw std::bad_alloc();
#endif
        }

        char *p = mPos;
        mPos += chunkSize;
        return p;
    }

    void deallocateImpl(void *p, size_t size) _ut_noexcept
    {
        ut_dcheck(isValidPos(mPos) &&
            "ArenaAlloc has outlived arena");

        ut_dcheck(p != nullptr);

        // Do nothing.

        //size_t chunkSize = (size + max_align_size - 1) / max_align_size * max_align_size;
        //
        //if (static_cast<char*>(p) + chunkSize == mPos) // safe cast
        //    mPos = static_cast<char*>(p); // safe cast
    }
};

//
// StackArena
//

template <size_t N>
class LinearStackArena : public LinearBufferArena
{
public:
    static const size_t buffer_capacity = (N + max_align_size - 1) /
            max_align_size * max_align_size; // round up

    LinearStackArena() _ut_noexcept
        : LinearBufferArena(
            ptrCast<char*>(&mStorage), // safe cast
            buffer_capacity) { }

private:
    MaxAlignedStorage<buffer_capacity> mStorage;
};

}
