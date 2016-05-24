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

#include "impl/Common.h"
#include "impl/Assert.h"
#include "util/SmartPtr.h"
#include "util/VirtualObject.h"
#include <exception>
#include <type_traits>

namespace ut {

//
// type-erased coroutine handle
//

class Coroutine
{
public:
    template <class T>
    static Coroutine wrap(T&& core)
    {
        static_assert(std::is_rvalue_reference<T&&>::value,
            "Argument expected to be an rvalue");

        static_assert(IsNoThrowMovable<T>::value,
            "Wrapped type may not throw on move construction or assignment. "
            "Add noexcept or throw() specifier");

        return Coroutine(InPlaceTag(), std::move(core));
    }

    Coroutine() _ut_noexcept { }

    Coroutine(Coroutine&& other) _ut_noexcept
        : mData(std::move(other.mData)) { }

    Coroutine& operator=(Coroutine&& other) _ut_noexcept
    {
        mData = std::move(other.mData);

        return *this;
    }

    bool operator()(void *arg = nullptr)
    {
        return (*mData)(arg);
    }

    bool isValid() const _ut_noexcept
    {
        return (bool) mData;
    }

    bool isDone() const _ut_noexcept
    {
        return (*mData).isDone();
    }

    void* value() const _ut_noexcept
    {
        return (*mData).value();
    }

    template <typename T>
    T& valueAs() const _ut_noexcept
    {
        ut_dcheck(value() != nullptr);

        return *static_cast<T*>(value()); // safe cast if T is original type
    }

private:
    Coroutine(const Coroutine& other) = delete;
    Coroutine& operator=(const Coroutine& other) = delete;

    struct IAdapter : IVirtual
    {
        virtual bool operator()(void *arg) = 0;

        virtual bool isDone() const _ut_noexcept = 0;

        virtual void* value() const _ut_noexcept = 0;
    };

    template <class T>
    struct Adapter
        : public UniqueMixin<IAdapter, Adapter<T>>
    {
        Adapter(T&& core)
            : mCore(std::move(core)) { }

        Adapter(Adapter&& other) _ut_noexcept
            : mCore(std::move(other.mCore)) { }

        bool operator()(void *arg) final
        {
            return mCore(arg);
        }

        bool isDone() const _ut_noexcept final
        {
            return mCore.isDone();
        }

        void* value() const _ut_noexcept final
        {
            return mCore.value();
        }

    private:
        T mCore;
    };

    template <class T>
    Coroutine(InPlaceTag, T&& core) _ut_noexcept
        : mData(TypeInPlaceTag<Adapter<T>>(), std::move(core)) { }

    // Reserve space for two pointers (virtual table ptr + managed data).
    ut::VirtualObjectData<IAdapter,
        virtual_header_size + ptr_size, ptr_size> mData;
};

}
