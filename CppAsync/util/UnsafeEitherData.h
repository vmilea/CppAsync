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
#include "Meta.h"
#include <utility>

namespace ut {

struct ATag { };
struct BTag { };

template <class A, class B>
class UnsafeEitherData
{
public:
    using a_type = A;
    using b_type = B;

    template <EnableIf<std::is_default_constructible<A>::value> = nullptr>
    UnsafeEitherData()
    {
        constructA();
    }

    template <class ...Args>
    explicit UnsafeEitherData(ATag /* target */, Args&&... args)
    {
        constructA(std::forward<Args>(args)...);
    }

    template <class ...Args>
    explicit UnsafeEitherData(BTag /* target */, Args&&... args)
    {
        constructB(std::forward<Args>(args)...);
    }

    UnsafeEitherData(const A& value)
    {
        constructA(value);
    }

    UnsafeEitherData(A&& value)
    {
        constructA(std::move(value));
    }

    UnsafeEitherData(const B& value)
    {
        constructB(value);
    }

    UnsafeEitherData(B&& value)
    {
        constructB(std::move(value));
    }

    UnsafeEitherData(ATag /* other */, const UnsafeEitherData<A, B>& other)
    {
        constructA(other.a());
    }

    UnsafeEitherData(BTag /* other */, const UnsafeEitherData<A, B>& other)
    {
        constructB(other.b());
    }

    UnsafeEitherData(bool isOtherB, const UnsafeEitherData<A, B>& other)
    {
        if (isOtherB)
            constructB(other.b());
        else
            constructA(other.a());
    }

    UnsafeEitherData(ATag /* other */, UnsafeEitherData<A, B>&& other)
    {
        constructA(std::move(other.a()));
    }

    UnsafeEitherData(BTag /* other */, UnsafeEitherData<A, B>&& other)
    {
        constructB(std::move(other.b()));
    }

    UnsafeEitherData(bool isOtherB, UnsafeEitherData<A, B>&& other)
    {
        if (isOtherB)
            constructB(std::move(other.b()));
        else
            constructA(std::move(other.a()));
    }

    void destructA() _ut_noexcept
    {
        a().~A();
    }

    void destructB() _ut_noexcept
    {
        b().~B();
    }

    void destruct(bool isB) _ut_noexcept
    {
        if (isB)
            destructB();
        else
            destructA();
    }

    void assignIntoBlank(const A& value)
    {
        constructA(value);
    }

    void assignIntoBlank(A&& value)
    {
        constructA(std::move(value));
    }

    void assignIntoBlank(const B& value)
    {
        constructB(value);
    }

    void assignIntoBlank(B&& value)
    {
        constructB(std::move(value));
    }

    void assignIntoA(const A& value)
    {
        a() = value;
    }

    void assignIntoA(A&& value)
    {
        a() = std::move(value);
    }

    // Note: blank after throw!
    void assignIntoA(const B& value)
    {
        a().~A();
        constructB(value);
    }

    // Note: blank after throw!
    void assignIntoA(B&& value)
    {
        a().~A();
        constructB(std::move(value));
    }

    // Note: blank after throw!
    void assignIntoB(const A& value)
    {
        b().~B();
        constructA(value);
    }

    // Note: blank after throw!
    void assignIntoB(A&& value)
    {
        b().~B();
        constructA(std::move(value));
    }

    void assignIntoB(const B& value)
    {
        b() = value;
    }

    void assignIntoB(B&& value)
    {
        b() = std::move(value);
    }

    // Note: blank after throw!
    void assign(bool isB, const A& value)
    {
        if (isB)
            assignIntoB(value);
        else
            assignIntoA(value);
    }

    // Note: blank after throw!
    void assign(bool isB, A&& value)
    {
        if (isB)
            assignIntoB(std::move(value));
        else
            assignIntoA(std::move(value));
    }

    // Note: blank after throw!
    void assign(bool isB, const B& value)
    {
        if (isB)
            assignIntoB(value);
        else
            assignIntoA(value);
    }

    // Note: blank after throw!
    void assign(bool isB, B&& value)
    {
        if (isB)
            assignIntoB(std::move(value));
        else
            assignIntoA(std::move(value));
    }

    void assignAIntoA(const UnsafeEitherData<A, B>& other)
    {
        a() = other.a();
    }

    // Note: blank after throw!
    void assignBIntoA(const UnsafeEitherData<A, B>& other)
    {
        a().~A();
        constructB(other.b());
    }

    // Note: blank after throw!
    void assignAIntoB(const UnsafeEitherData<A, B>& other)
    {
        b().~B();
        constructA(other.a());
    }

    void assignBIntoB(const UnsafeEitherData<A, B>& other)
    {
        b() = other.b();
    }

    // Note: blank after throw!
    void assign(bool isB, bool isOtherB, const UnsafeEitherData<A, B>& other)
    {
        if (isB) {
            if (isOtherB)
                assignBIntoB(other);
            else
                assignAIntoB(other);
        } else {
            if (isOtherB)
                assignBIntoA(other);
            else
                assignAIntoA(other);
        }
    }

    void assignAIntoA(UnsafeEitherData<A, B>&& other)
    {
        a() = std::move(other.a());
    }

    // Note: blank after throw!
    void assignBIntoA(UnsafeEitherData<A, B>&& other)
    {
        a().~A();
        constructB(std::move(other.b()));
    }

    // Note: blank after throw!
    void assignAIntoB(UnsafeEitherData<A, B>&& other)
    {
        b().~B();
        constructA(std::move(other.a()));
    }

    void assignBIntoB(UnsafeEitherData<A, B>&& other)
    {
        b() = std::move(other.b());
    }

    // Note: blank after throw!
    void assign(bool isB, bool isOtherB, UnsafeEitherData<A, B>&& other)
    {
        if (isB) {
            if (isOtherB)
                assignBIntoB(std::move(other));
            else
                assignAIntoB(std::move(other));
        } else {
            if (isOtherB)
                assignBIntoA(std::move(other));
            else
                assignAIntoA(std::move(other));
        }
    }

    // Note: blank after throw!
    template <class ...Args>
    void emplaceAIntoA(Args&&... args)
    {
        a().~A();
        constructA(std::forward<Args>(args)...);
    }

    // Note: blank after throw!
    template <class ...Args>
    void emplaceBIntoA(Args&&... args)
    {
        a().~A();
        constructB(std::forward<Args>(args)...);
    }

    // Note: blank after throw!
    template <class ...Args>
    void emplaceAIntoB(Args&&... args)
    {
        b().~B();
        constructA(std::forward<Args>(args)...);
    }

    // Note: blank after throw!
    template <class ...Args>
    void emplaceBIntoB(Args&&... args)
    {
        b().~B();
        constructB(std::forward<Args>(args)...);
    }

    // Note: blank after throw!
    template <class ...Args>
    void emplaceA(bool isB, Args&&... args)
    {
        if (isB)
            emplaceAIntoB(std::forward<Args>(args)...);
        else
            emplaceAIntoA(std::forward<Args>(args)...);
    }

    // Note: blank after throw!
    template <class ...Args>
    void emplaceB(bool isB, Args&&... args)
    {
        if (isB)
            emplaceBIntoB(std::forward<Args>(args)...);
        else
            emplaceBIntoA(std::forward<Args>(args)...);
    }

    void swapAIntoA(UnsafeEitherData& other)
    {
        using std::swap;
        swap(a(), other.a());
    }

    // Note: blank after throw!
    void swapBIntoA(UnsafeEitherData& other)
    {
        static_assert(std::is_nothrow_move_constructible<A>::value,
            "Swap between different kinds is supported only when A is no-throw move constructible");

        A tmp(std::move(a()));
        a().~A();
        constructB(std::move(other.b())); // may throw
        other.b().~B();
        other.constructA(std::move(tmp));
    }

    // Note: blank after throw!
    void swapAIntoB(UnsafeEitherData& other)
    {
        other.swapBIntoA(*this);
    }

    void swapBIntoB(UnsafeEitherData& other)
    {
        using std::swap;
        swap(b(), other.b());
    }

    // Note: blank after throw!
    void swap(bool isB, bool isOtherB, UnsafeEitherData& other)
    {
        if (isB) {
            if (isOtherB)
                swapBIntoB(other);
            else
                swapAIntoB(other);
        } else {
            if (isOtherB)
                swapBIntoA(other);
            else
                swapAIntoA(other);
        }
    }

    const A& a() const _ut_noexcept
    {
        return ptrCast<const A&>(mData); // safe cast
    }

    A& a() _ut_noexcept
    {
        return ptrCast<A&>(mData); // safe cast
    }

    const B& b() const _ut_noexcept
    {
        return ptrCast<const B&>(mData); // safe cast
    }

    B& b() _ut_noexcept
    {
        return ptrCast<B&>(mData); // safe cast
    }

private:
    UnsafeEitherData(const UnsafeEitherData& other) = delete;
    UnsafeEitherData& operator=(const UnsafeEitherData& other) = delete;

    static const std::size_t data_size = Max<
        sizeof(A),
        sizeof(B)>::value;

    static const std::size_t data_alignment = Max<
        std::alignment_of<A>::value,
        std::alignment_of<B>::value>::value;

    template <class ...Args>
    void constructA(Args&&... args)
    {
        new (&mData) A(std::forward<Args>(args)...);
    }

    template <class ...Args>
    void constructB(Args&&... args)
    {
        new (&mData) B(std::forward<Args>(args)...);
    }

    AlignedStorage<data_size, data_alignment> mData;
};

}
