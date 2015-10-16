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
#include "Mixin.h"
#include "SmartPtr.h"
#include "TypeTraits.h"

namespace ut {

namespace detail
{
    template <class T, class Alloc,
        bool IsStatelessAllocator = std::is_empty<Alloc>::value>
    class AllocElementData : private T
    {
    public:
        using data_allocator_type = RebindAlloc<Alloc, AllocElementData>;

        static AllocElementData& getByCore(T& core) _ut_noexcept
        {
            return static_cast<AllocElementData&>(core); // safe cast
        }

        template <class ...Args>
        AllocElementData(const Alloc& alloc, Args&&... args)
            : T(std::forward<Args>(args)...)
            , mAlloc(alloc)
        {
            // Expecting match even if fields are not standard-layout
            ut_assert(this == &getByCore(core()));
        }

        const T& core() const _ut_noexcept
        {
            return *this;
        }

        T& core() _ut_noexcept
        {
            return *this;
        }

        const data_allocator_type& allocator() const _ut_noexcept
        {
            return mAlloc;
        }

    private:
        data_allocator_type mAlloc;
    };

    // Empty base class optimization
    template <class T, class Alloc>
    class AllocElementData<T, Alloc, true>
        : private RebindAlloc<Alloc, AllocElementData<T, Alloc, true>>
        , private T
    {
    public:
        using data_allocator_type = RebindAlloc<Alloc, AllocElementData>;

        static AllocElementData& getByCore(T& core) _ut_noexcept
        {
            return static_cast<AllocElementData&>(core); // safe cast
        }

        template <class ...Args>
        AllocElementData(const Alloc& alloc, Args&&... args)
            : data_allocator_type(alloc)
            , T(std::forward<Args>(args)...)
        {
            // Expecting match even if fields are not standard-layout
            ut_assert(this == &getByCore(core()));
        }

        const T& core() const _ut_noexcept
        {
            return *this;
        }

        T& core() _ut_noexcept
        {
            return *this;
        }

        const data_allocator_type& allocator() const _ut_noexcept
        {
            return *this;
        }
    };
}

//
// AllocElementPtr allocates and manages a unique object. It has similar
// semantics to unique_ptr.
//

template <class T, class Alloc>
class AllocElementPtr
{
#ifndef _MSC_VER // MSVC doesn't optimize empty base class reliably
    static_assert(!std::is_empty<Alloc>::value
        || sizeof(T) == sizeof(detail::AllocElementData<T, Alloc, true>),
        "Empty base class optimization not being performed");
#endif

protected:
    using data_type = detail::AllocElementData<T, Alloc>;
    using data_allocator_type = typename data_type::data_allocator_type;
    using data_allocator_traits = std::allocator_traits<data_allocator_type>;

public:
    using element_type = T;
    using allocator_type = data_allocator_type;

    static AllocElementPtr restoreFromCore(T& core)
    {
        AllocElementPtr ptr;
        ptr.mData = &data_type::getByCore(core);
        return ptr;
    }

    AllocElementPtr() _ut_noexcept
        : mData(nullptr) { }

    template <class ...Args>
    AllocElementPtr(const Alloc& alloc, Args&&... args)
    {
        data_allocator_type a(alloc);
        mData = data_allocator_traits::allocate(a, 1);

#ifdef UT_DISABLE_EXCEPTIONS
        if (mData == nullptr) {
            // Construct nil ptr.
        } else {
            data_allocator_traits::construct(a, mData,
                alloc, std::forward<Args>(args)...);
        }
#else
        ut_assert(mData != nullptr &&
            "Allocator may not return nullptr");

        try {
            data_allocator_traits::construct(a, mData,
                alloc, std::forward<Args>(args)...);
        } catch (...) {
            data_allocator_traits::deallocate(a, mData, 1);
            throw;
        }
#endif
    }

    AllocElementPtr(AllocElementPtr&& other) _ut_noexcept
        : mData(movePtr(other.mData)) { }

    AllocElementPtr& operator=(AllocElementPtr&& other) _ut_noexcept
    {
        ut_dcheck(this != &other);

        reset();
        mData = movePtr(other.mData);

        return *this;
    }

    ~AllocElementPtr() _ut_noexcept
    {
        reset();
    }

    bool isNil() const _ut_noexcept
    {
        return mData == nullptr;
    }

    explicit operator bool() const _ut_noexcept
    {
        return !isNil();
    }

    const T* get() const _ut_noexcept
    {
        return isNil() ? nullptr : &mData->core();
    }

    T* get() _ut_noexcept
    {
        return isNil() ? nullptr : &mData->core();
    }

    const T* operator->() const _ut_noexcept
    {
        return &mData->core();
    }

    T* operator->() _ut_noexcept
    {
        return &mData->core();
    }

    const T& operator*() const _ut_noexcept
    {
        return mData->core();
    }

    T& operator*() _ut_noexcept
    {
        return mData->core();
    }

    void reset() _ut_noexcept
    {
        if (!isNil()) {
            data_type *data = movePtr(mData);
            data_allocator_type a = data->allocator();
            data_allocator_traits::destroy(a, data);
            data_allocator_traits::deallocate(a, data, 1);
        }
    }

    T* release() _ut_noexcept
    {
        if (isNil()) {
            return nullptr;
        } else {
            T* core = &mData->core();
            mData = nullptr;
            return core;
        }
    }

    void swap(AllocElementPtr& other) _ut_noexcept
    {
        std::swap(mData, other.mData);
    }

protected:
    data_type *mData;
};

template <class T, class Alloc>
void swap(AllocElementPtr<T, Alloc>& a, AllocElementPtr<T, Alloc>& b) _ut_noexcept
{
    a.swap(b);
}

//
// Mixin version
//

template <class Derived, class T>
struct AllocElementMixin
{
    const T& core() const _ut_noexcept
    {
        return *static_cast<const Derived&>(*this); // safe cast
    }

    T& core() _ut_noexcept
    {
        return *static_cast<Derived&>(*this); // safe cast
    }
};

template <class T, class Alloc, template<class, class> class Mixin>
class ExtendedAllocElementPtr
    : public AllocElementPtr<T, Alloc>
    , public Mixin<ExtendedAllocElementPtr<T, Alloc, Mixin>, T>
{
private:
    using base_type = AllocElementPtr<T, Alloc>;

    static_assert(std::is_empty<Mixin<base_type, T>>::value,
        "Mixin should be an empty type");

public:
    using mixin_type = Mixin<ExtendedAllocElementPtr<T, Alloc, Mixin>, T>;

    static ExtendedAllocElementPtr restoreFromCore(T& core)
    {
        using data_type = typename base_type::data_type;

        ExtendedAllocElementPtr ptr;
        ptr.mData = &data_type::getByCore(core);
        return ptr;
    }

    ExtendedAllocElementPtr() _ut_noexcept
    {
        static_assert(sizeof(ExtendedAllocElementPtr) == sizeof(base_type),
            "Empty base class optimization not being performed");
    }

    template <class ...Args>
    ExtendedAllocElementPtr(const Alloc& alloc, Args&&... args)
        : base_type(alloc, std::forward<Args>(args)...) { }

    ExtendedAllocElementPtr(ExtendedAllocElementPtr&& other) _ut_noexcept
        : base_type(static_cast<base_type&&>(other)) { } // safe cast

    ExtendedAllocElementPtr& operator=(ExtendedAllocElementPtr&& other) _ut_noexcept
    {
        base_type::operator=(std::move(other));

        return *this;
    }
};

template <class T, class Alloc, template<class, class> class Mixin>
void swap(ExtendedAllocElementPtr<T, Alloc, Mixin>& a,
    ExtendedAllocElementPtr<T, Alloc, Mixin>& b) _ut_noexcept
{
    a.swap(b);
}

//
// Instance generators
//

template <class T, class Alloc, class ...Args>
AllocElementPtr<T, Alloc> makeAllocElementPtr(const Alloc& alloc, Args&&... args)
{
    return AllocElementPtr<T, Alloc>(alloc, std::forward<Args>(args)...);
}

template <class T, template<class, class> class Mixin, class Alloc, class ...Args>
ExtendedAllocElementPtr<T, Alloc, Mixin> makeExtendedAllocElementPtr(const Alloc& alloc,
    Args&&... args)
{
    return ExtendedAllocElementPtr<T, Alloc, Mixin>(alloc, std::forward<Args>(args)...);
}

}
