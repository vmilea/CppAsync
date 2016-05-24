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
#include "Meta.h"

namespace ut {

template <class T, class Alloc = std::allocator<char>>
class ContextRef
{
public:
    using value_type = T;
    using alloc_type = Alloc;

    ContextRef() _ut_noexcept { }

    explicit ContextRef(const Alloc& alloc) _ut_noexcept
        : mAlloc(alloc) { }

    template <class ...Args>
    ContextRef(std::shared_ptr<void> parent, const Alloc& alloc, Args&&... args)
        : mNode(std::allocate_shared<Node>(alloc, std::move(parent), std::forward<Args>(args)...))
        , mAlloc(alloc) { }

    ContextRef(const ContextRef& other) _ut_noexcept
        : mNode(other.mNode)
        , mAlloc(other.mAlloc) { }

    ContextRef(ContextRef&& other) _ut_noexcept
        : mNode(std::move(other.mNode))
        , mAlloc(std::move(other.mAlloc)) { }

    ContextRef& operator=(const ContextRef& other) _ut_noexcept
    {
        mNode = other.mNode;
        mAlloc = other.mAlloc;

        return *this;
    }

    ContextRef& operator=(ContextRef&& other) _ut_noexcept
    {
        ut_assert(this != &other);

        mNode = std::move(other.mNode);
        mAlloc = std::move(other.mAlloc);

        return *this;
    }

    void swap(ContextRef& other) _ut_noexcept
    {
        using std::swap;

        swap(mNode, other.mNode);
        swap(mAlloc, other.mAlloc);
    }

    template <class U, class ...Args>
    ContextRef<U, Alloc> spawn(Args&&... args) const
    {
        return ContextRef<U, Alloc>(mNode, mAlloc, std::forward<Args>(args)...);
    }

    template <class U, class UAlloc, class ...Args>
    ContextRef<U, UAlloc> spawnWithAllocator(const UAlloc& alloc, Args&&... args) const
    {
        return ContextRef<U, UAlloc>(mNode, alloc, std::forward<Args>(args)...);
    }

    std::shared_ptr<void> ptr() const _ut_noexcept
    {
        return mNode;
    }

    Alloc allocator() const _ut_noexcept
    {
        return mAlloc;
    }

    const T* get() const _ut_noexcept
    {
        return mNode == nullptr ? nullptr : &mNode->value;
    }

    T* get() _ut_noexcept
    {
        return mNode == nullptr ? nullptr : &mNode->value;
    }

    const T& operator*() const _ut_noexcept
    {
        ut_dcheck(mNode != nullptr);

        return mNode->value;
    }

    T& operator*() _ut_noexcept
    {
        ut_dcheck(mNode != nullptr);

        return mNode->value;
    }

    const T* operator->() const _ut_noexcept
    {
        ut_dcheck(mNode != nullptr);

        return &mNode->value;
    }

    T* operator->() _ut_noexcept
    {
        ut_dcheck(mNode != nullptr);

        return &mNode->value;
    }

    explicit operator bool() const _ut_noexcept
    {
        return mNode != nullptr;
    }

    ContextRef<void, Alloc> operator()() const _ut_noexcept
    {
        return ContextRef<void, Alloc>(*this);
    }

private:
    struct Node
    {
        std::shared_ptr<void> parent;
        T value;

        template <class ...Args>
        Node(std::shared_ptr<void> parent, Args&&... args)
            : parent(std::move(parent))
            , value(std::forward<Args>(args)...) { }
    };

    std::shared_ptr<Node> mNode;
    Alloc mAlloc;

    template <class U, class UAlloc>
    friend class ContextRef;
};

template <class Alloc>
class ContextRef<void, Alloc>
{
public:
    using value_type = void;
    using alloc_type = Alloc;

    ContextRef() _ut_noexcept { }

    explicit ContextRef(const Alloc& alloc) _ut_noexcept
        : mAlloc(alloc) { }

    ContextRef(std::shared_ptr<void> parent, const Alloc& alloc) _ut_noexcept
        : mNode(std::move(parent))
        , mAlloc(alloc) { }

    ContextRef(std::shared_ptr<void> parent) _ut_noexcept // implicit
        : ContextRef(parent, Alloc()) { }

    template <class U>
    ContextRef(const ContextRef<U, Alloc>& other) _ut_noexcept
        : mNode(other.mNode)
        , mAlloc(other.mAlloc) { }

    ContextRef(const ContextRef& other) _ut_noexcept
        : mNode(other.mNode)
        , mAlloc(other.mAlloc) { }

    template <class U>
    ContextRef(ContextRef<U, Alloc>&& other) _ut_noexcept
        : mNode(std::move(other.mNode))
        , mAlloc(std::move(other.mAlloc)) { }

    ContextRef(ContextRef&& other) _ut_noexcept
        : mNode(std::move(other.mNode))
        , mAlloc(std::move(other.mAlloc)) { }

    template <class U>
    ContextRef& operator=(const ContextRef<U, Alloc>& other) _ut_noexcept
    {
        mNode = other.mNode;
        mAlloc = other.mAlloc;

        return *this;
    }

    ContextRef& operator=(const ContextRef& other) _ut_noexcept
    {
        mNode = other.mNode;
        mAlloc = other.mAlloc;

        return *this;
    }

    template <class U>
    ContextRef& operator=(ContextRef<U, Alloc>&& other) _ut_noexcept
    {
        ut_assert(this != &other);

        mNode = std::move(other.mNode);
        mAlloc = std::move(other.mAlloc);

        return *this;
    }

    ContextRef& operator=(ContextRef&& other) _ut_noexcept
    {
        ut_assert(this != &other);

        mNode = std::move(other.mNode);
        mAlloc = std::move(other.mAlloc);

        return *this;
    }

    ContextRef& operator=(std::shared_ptr<void> parent) _ut_noexcept
    {
        mNode = std::move(parent);

        return *this;
    }

    void swap(ContextRef& other) _ut_noexcept
    {
        using std::swap;

        swap(mNode, other.mNode);
        swap(mAlloc, other.mAlloc);
    }

    template <class U, class ...Args>
    ContextRef<U, Alloc> spawn(Args&&... args) const
    {
        return ContextRef<U, Alloc>(mNode, mAlloc, std::forward<Args>(args)...);
    }

    template <class U, class UAlloc, class ...Args>
    ContextRef<U, UAlloc> spawnWithAllocator(const UAlloc& alloc, Args&&... args) const
    {
        return ContextRef<U, UAlloc>(mNode, alloc, std::forward<Args>(args)...);
    }

    std::shared_ptr<void> ptr() const _ut_noexcept
    {
        return mNode;
    }

    Alloc allocator() const _ut_noexcept
    {
        return mAlloc;
    }

    explicit operator bool() const _ut_noexcept
    {
        return mNode != nullptr;
    }

    ContextRef<void, Alloc> operator()() const _ut_noexcept
    {
        return *this;
    }

private:
    std::shared_ptr<void> mNode;
    Alloc mAlloc;
};

template <class T, class Alloc>
bool operator==(const ContextRef<T, Alloc>& a, std::nullptr_t) _ut_noexcept
{
    return !a;
}

template <class T, class Alloc>
bool operator==(std::nullptr_t, const ContextRef<T, Alloc>& b) _ut_noexcept
{
    return b == nullptr;
}

template <class T, class Alloc>
bool operator!=(const ContextRef<T, Alloc>& a, std::nullptr_t) _ut_noexcept
{
    return !(a == nullptr);
}

template <class T, class Alloc>
bool operator!=(std::nullptr_t, const ContextRef<T, Alloc>& b) _ut_noexcept
{
    return !(b == nullptr);
}

template <class T, class Alloc>
void swap(ContextRef<T, Alloc>& a, ContextRef<T, Alloc>& b) _ut_noexcept
{
    a.swap(b);
}

//
// Instance generators
//

template <class T, class Alloc, class ...Args>
inline ContextRef<T, Alloc> makeContextWithAllocator(const Alloc& alloc, Args&&... args)
{
    return ContextRef<T, Alloc>(std::shared_ptr<void>(), alloc, std::forward<Args>(args)...);
}

template <class T, class Alloc = std::allocator<char>, class ...Args>
inline ContextRef<T, Alloc> makeContext(Args&&... args)
{
    return makeContextWithAllocator<T>(Alloc(), std::forward<Args>(args)...);
}

}
