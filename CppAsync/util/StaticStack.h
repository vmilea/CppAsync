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
#include "../util/Cast.h"
#include "TypeTraits.h"

namespace ut {

template <class T, int Capacity, bool HasTrivialType = std::is_trivial<T>::value>
class StaticStack;

template <class T, int Capacity, bool HasTrivialType>
class StaticStack
{
public:
    static_assert(Capacity > 0, "Invalid capacity");

    static const int capacity = Capacity;

    StaticStack() _ut_noexcept
        : mSize(0) { }

    ~StaticStack() _ut_noexcept
    {
        while (!isEmpty())
            pop();
    }

    bool isEmpty() const _ut_noexcept
    {
        return mSize == 0;
    }

    bool isFull() const _ut_noexcept
    {
        return mSize == capacity;
    }

    std::size_t size() const _ut_noexcept
    {
        return mSize;
    }

    bool contains(const T& value) const
    {
        for (std::size_t i = 0; i < mSize; i++) {
            if (data(i) == value)
                return true;
        }
        return false;
    }

    const T& top() const _ut_noexcept
    {
        ut_dcheck(!isEmpty());

        return data(mSize - 1);
    }

    T& top() _ut_noexcept
    {
        ut_dcheck(!isEmpty());

        return data(mSize - 1);
    }

    void push(const T& value)
    {
        ut_dcheck(mSize < capacity);

        T& item = data(mSize);
        new (&item) T(value);
        mSize++;
    }

    void push(T&& value)
    {
        ut_dcheck(mSize < capacity);

        T& item = data(mSize);
        new (&item) T(std::move(value));
        mSize++;
    }

    template <class ...Args>
    void emplace(Args&&... args)
    {
        ut_dcheck(mSize < capacity);

        T& item = data(mSize);
        new (&item) T(std::forward<Args>(args)...);
        mSize++;
    }

    void pop() _ut_noexcept
    {
        ut_dcheck(mSize > 0);

        T& item = data(--mSize);
        item.~T();
    }

private:
    StaticStack(const StaticStack& other) = delete;
    StaticStack& operator=(const StaticStack& other) = delete;

    const T& data(std::size_t index) const _ut_noexcept
    {
        return ptrCast<const T*>(&mData)[index]; // safe cast
    }

    T& data(std::size_t index) _ut_noexcept
    {
        return ptrCast<T*>(&mData)[index]; // safe cast
    }

    AlignedStorage<Capacity * sizeof(T), std::alignment_of<T>::value> mData;
    std::size_t mSize;
};

template <class T, int Capacity>
class StaticStack<T, Capacity, true>
{
public:
    static_assert(Capacity > 0, "Invalid capacity");

    static const int capacity = Capacity;

    StaticStack() _ut_noexcept
        : mSize(0) { }

    bool isEmpty() const _ut_noexcept
    {
        return mSize == 0;
    }

    bool isFull() const _ut_noexcept
    {
        return mSize == capacity;
    }

    std::size_t size() const _ut_noexcept
    {
        return mSize;
    }

    bool contains(const T& value) const _ut_noexcept
    {
        for (std::size_t i = 0; i < mSize; i++) {
            if (mData[i] == value)
                return true;
        }
        return false;
    }

    const T& top() const _ut_noexcept
    {
        ut_dcheck(!isEmpty());

        return mData[mSize - 1];
    }

    T& top() _ut_noexcept
    {
        ut_dcheck(!isEmpty());

        return mData[mSize - 1];
    }

    void push(T value) _ut_noexcept
    {
        ut_dcheck(mSize < capacity);

        mData[mSize++] = value;
    }

    template <class ...Args>
    void emplace(Args&&... args) _ut_noexcept
    {
        ut_dcheck(mSize < capacity);

        mData[mSize++] = T(std::forward<Args>(args)...);
    }

    void pop() _ut_noexcept
    {
        ut_dcheck(mSize > 0);

        mSize--;
    }

private:
    StaticStack(const StaticStack& other) = delete;
    StaticStack& operator=(const StaticStack& other) = delete;

    T mData[Capacity];
    std::size_t mSize;
};

}
