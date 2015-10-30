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

/**
 * @file  VirtualObject.h
 *
 * Helper class for type erasure
 *
 */

#pragma once

#include "../impl/Common.h"
#include "../impl/Assert.h"
#include "Cast.h"
#include "Meta.h"

namespace ut {

namespace detail
{
    template <class C>
    class HasExactMethod_clone_pvoid
    {
        template <class U> static std::true_type testSignature(void (U::*)(void *) const);
        template <class U> static decltype(testSignature(&U::clone)) test(std::nullptr_t);
        template <class U> static std::false_type test(...);

    public:
        using type = decltype(test<C>(nullptr));
        static const bool value = type::value;
    };

    template <class C>
    class HasExactMethod_move_pvoid
    {
        template <class U> static std::true_type testSignature(void (U::*)(void *));
        template <class U> static decltype(testSignature(&U::move)) test(std::nullptr_t);
        template <class U> static std::false_type test(...);

    public:
        using type = decltype(test<C>(nullptr));
        static const bool value = type::value;
    };

    template <class Interface>
    struct VirtualObjectTraits
    {
    private:
        static_assert(std::is_abstract<Interface>::value,
            "Interface should be abstract");

        static_assert(sizeof(Interface) == virtual_header_size,
            "Interface may not have internal state or be involved in multiple inheritance");

        static_assert(HasExactMethod_clone_pvoid<Interface>::value,
            "Interface must declare method: virtual void clone(void *into) const");

        static_assert(HasExactMethod_move_pvoid<Interface>::value,
            "Interface must declare method: virtual void move(void *into) noexcept");

    public:
        static const bool valid = true;
    };
}

template <class Interface, class Derived>
class PinnedMixin : public Interface
{
public:
    PinnedMixin() = default;

    void clone(void * /* into */) const final
    {
        ut_check(false); // PinnedMixin doesn't support cloning
    }

    void move(void * /* into */) _ut_noexcept final
    {
        ut_check(false); // PinnedMixin doesn't support moving
    }

private:
    PinnedMixin(const PinnedMixin& other) = delete;
    PinnedMixin& operator=(const PinnedMixin& other) = delete;
};

template <class Interface, class Derived, bool enforce_nothrow_move = true>
class MovableMixin : public Interface
{
public:
    // static_assert(std::is_move_constructible<Derived>::value,
    //     "Derived type must be move constructible");

    MovableMixin() = default;

    MovableMixin(MovableMixin&& /* other */) _ut_noexcept { }

    MovableMixin& operator=(MovableMixin&& /* other */) _ut_noexcept
    {
        return *this;
    }

    void clone(void * /* into */) const final
    {
        ut_check(false); // MovableMixin supports moving but not cloning
    }

    void move(void *into) _ut_noexcept final
    {
        static_assert(std::is_move_constructible<Derived>::value,
            "Type must be move constructible");

        static_assert(!enforce_nothrow_move || std::is_nothrow_move_constructible<Derived>::value,
            "Type may not throw from move constructor. "
            "Add noexcept / throw() specifier, or disable enforce_nothrow_move");

        new (into) Derived(static_cast<Derived&&>(*this)); // safe cast
    }

private:
    MovableMixin(const MovableMixin& other) = delete;
    MovableMixin& operator=(const MovableMixin& other) = delete;
};

template <class Interface, class Derived, bool enforce_nothrow_move = true>
class CopyableMovableMixin : public Interface
{
public:
    // static_assert(IsCopyAndMoveConstructible<Derived>::value,
    //     "Derived type must be copy constructible and move constructible");

    CopyableMovableMixin() = default;

    void clone(void *into) const final
    {
        static_assert(std::is_copy_constructible<Derived>::value,
            "Type must be copy constructible");

        new (into) Derived(static_cast<const Derived&>(*this)); // safe cast
    }

    void move(void *into) _ut_noexcept final
    {
        static_assert(std::is_move_constructible<Derived>::value,
            "Type must be move constructible");

        static_assert(!enforce_nothrow_move || std::is_nothrow_move_constructible<Derived>::value,
            "Type may not throw from move constructor. "
            "Add noexcept / throw() specifier, or disable enforce_nothrow_move");

        new (into) Derived(static_cast<Derived&&>(*this)); // safe cast
    }
};

template <class Interface, std::size_t Len, std::size_t Align>
class VirtualObjectData
{
public:
    using interface_type = Interface;
    using data_type = ut::AlignedStorage<Len, Align>;

    static_assert(detail::VirtualObjectTraits<Interface>::valid, "");

    VirtualObjectData() _ut_noexcept
    {
        clear();
    }

    template <class T, class ...Args>
    VirtualObjectData(TypeInPlaceTag<T>, Args&&... args)
    {
        static_assert(!std::is_abstract<T>::value,
                "Emplaced type may not be abstract");

        static_assert(std::is_base_of<Interface, T>::value,
            "Emplaced type must derive from Interface");

        static_assert(sizeof(T) <= sizeof(data_type),
            "Emplaced type is too large");
        static_assert(std::alignment_of<T>::value <= std::alignment_of<data_type>::value,
            "Emplaced type alignment is too large");

        new (&mData) T(std::forward<Args>(args)...);
    }

    VirtualObjectData(const VirtualObjectData& other)
    {
        if (!other.isNil())
            other->clone(&mData);
        else
            clear();
    }

    VirtualObjectData(VirtualObjectData&& other) _ut_noexcept
    {
        if (!other.isNil()) {
            other->move(&mData);
            other.destruct();
            other.clear();
        } else {
            clear();
        }
    }

    VirtualObjectData& operator=(const VirtualObjectData& other)
    {
        if (this != &other)
        {
            VirtualObjectData tmp;

#ifdef UT_NO_EXCEPTIONS
            other->clone(&tmp.mData);
#else
            try {
                other->clone(&tmp.mData);
            } catch (...) {
                // Copy has not been constructed, clear any junk
                // to ensure the destructor doesn't get called.
                tmp.clear();
                throw;
            }
#endif

            *this = std::move(tmp);
        }

        return *this;
    }

    VirtualObjectData& operator=(VirtualObjectData&& other) _ut_noexcept
    {
        ut_assert(this != &other);

        if (!isNil())
            destruct();

        if (!other.isNil()) {
            other->move(&mData);
            other.destruct();
            other.clear();
        } else {
            clear();
        }

        return *this;
    }

    ~VirtualObjectData() _ut_noexcept
    {
        if (!isNil())
            destruct();
    }

    void swap(VirtualObjectData& other) _ut_noexcept
    {
        std::swap(*this, other);
    }

    const Interface* get() const _ut_noexcept
    {
        return isNil() ? nullptr : operator->();
    }

    Interface* get() _ut_noexcept
    {
        return isNil() ? nullptr : operator->();
    }

    const Interface* operator->() const _ut_noexcept
    {
        ut_dcheck(!isNil());

        return ptrCast<const Interface*>(&mData); // safe cast
    }

    Interface* operator->() _ut_noexcept
    {
        ut_dcheck(!isNil());

        return ptrCast<Interface*>(&mData); // safe cast
    }

    const Interface& operator*() const _ut_noexcept
    {
        ut_dcheck(!isNil());

        return ptrCast<const Interface&>(mData); // safe cast
    }

    Interface& operator*() _ut_noexcept
    {
        ut_dcheck(!isNil());

        return ptrCast<Interface&>(mData); // safe cast
    }

    explicit operator bool() const _ut_noexcept
    {
        return !isNil();
    }

    void reset() _ut_noexcept
    {
        if (!isNil()) {
            destruct();
            clear();
        }

        ut_assert(isNil());
    }

private:
    bool isNil() const _ut_noexcept
    {
        return mVPtr == nullptr;
    }

    void clear() _ut_noexcept
    {
        // Clear vtable pointer.
        mVPtr = nullptr;
    }

    void destruct() _ut_noexcept
    {
        (*this)->~Interface();
    }

    union
    {
        data_type mData;
        void *mVPtr;
    };
};

template <class Interface, std::size_t Len, std::size_t Align>
void swap(VirtualObjectData<Interface, Len, Align>& a,
    VirtualObjectData<Interface, Len, Align>& b) _ut_noexcept
{
    a.swap(b);
}

//
// Recommended base class for virtual objects
//

class IVirtual
{
public:
    virtual ~IVirtual() _ut_noexcept { }

    virtual void clone(void *into) const = 0;

    virtual void move(void *into) _ut_noexcept = 0;

protected:
    IVirtual() _ut_noexcept { }
};

}
