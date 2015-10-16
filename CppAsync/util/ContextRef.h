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

namespace detail
{
    template <class T>
    struct ContextNode;

    template <>
    struct ContextNode<void>
    {
        std::shared_ptr<void> parent;

        ContextNode(const std::shared_ptr<void>& parent) _ut_noexcept
            : parent(parent) { }
    };

    template <class T>
    struct ContextNode : ContextNode<void>
    {
        T value;

        template <class ...Args>
        ContextNode(const std::shared_ptr<void>& parent, Args&&... args)
            : base_type(parent)
            , value(std::forward<Args>(args)...) { }

    private:
        using base_type = ContextNode<void>;
    };
}

template <class T, class Alloc>
struct ContextRefMixin;

template <class T, class Alloc = std::allocator<char>>
class ContextRef : public ContextRefMixin<T, Alloc>
{
public:
    using value_type = T;
    using alloc_type = Alloc;

    template <class ...Args>
    ContextRef(const std::shared_ptr<void>& parent, const Alloc& alloc, Args&&... args)
        : mNode(std::allocate_shared<detail::ContextNode<T>>(alloc,
            parent, std::forward<Args>(args)...))
        , mAlloc(alloc) { }

    template <class U,
        class Z = T, EnableIfVoid<Z> = nullptr>
    ContextRef(const ContextRef<U, Alloc>& other) _ut_noexcept
        : mNode(other.mNode)
        , mAlloc(other.mAlloc) { }

    ContextRef(const ContextRef& other) _ut_noexcept
        : mNode(other.mNode)
        , mAlloc(other.mAlloc) { }

    template <class U,
        class Z = T, EnableIfVoid<Z> = nullptr>
    ContextRef(ContextRef<U, Alloc>&& other) _ut_noexcept
        : mNode(std::move(other.mNode))
        , mAlloc(std::move(other.mAlloc)) { }

    ContextRef(ContextRef&& other) _ut_noexcept
        : mNode(std::move(other.mNode))
        , mAlloc(std::move(other.mAlloc)) { }

    template <class U,
        class Z = T, EnableIfVoid<Z> = nullptr>
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

    template <class U,
        class Z = T, EnableIfVoid<Z> = nullptr>
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

    std::shared_ptr<void> parentPtr() const _ut_noexcept
    {
        return mNode->parent;
    }

    std::shared_ptr<void> ptr() const _ut_noexcept
    {
        return mNode;
    }

    Alloc allocator() const _ut_noexcept
    {
        return mAlloc;
    }

private:
    std::shared_ptr<detail::ContextNode<T>> mNode;
    Alloc mAlloc;

    template <class U, class UAlloc>
    friend class ContextRef;

    friend struct ContextRefMixin<T, Alloc>;
};

template <class T, class Alloc>
struct ContextRefMixin
{
    ContextRef<void, Alloc> operator()() const _ut_noexcept
    {
        return ContextRef<void, Alloc>(thiz());
    }

    const T& operator*() const
    {
        return thiz().mNode->value;
    }

    T& operator*()
    {
        return thiz().mNode->value;
    }

    const T* operator->() const
    {
        return &thiz().mNode->value;
    }

    T* operator->()
    {
        return &thiz().mNode->value;
    }

private:
    const ContextRef<T, Alloc>& thiz() const
    {
        return static_cast<const ContextRef<T, Alloc>&>(*this); // safe cast
    }

    ContextRef<T, Alloc>& thiz()
    {
        return static_cast<ContextRef<T, Alloc>&>(*this); // safe cast
    }
};

template <class Alloc>
struct ContextRefMixin<void, Alloc>
{
    ContextRef<void, Alloc> operator()() const _ut_noexcept
    {
        return thiz();
    }

private:
    const ContextRef<void, Alloc>& thiz() const
    {
        return static_cast<const ContextRef<void, Alloc>&>(*this); // safe cast
    }
};

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
