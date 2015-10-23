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

#include "../Common.h"
#include <CppAsync/util/FunctionTraits.h>
#include <CppAsync/util/Meta.h>
#include <CppAsync/util/VirtualObject.h>
#include <functional>
#include <stdexcept>

namespace util {

namespace detail
{
    template <class R, class ...Args>
    struct IFunction : ut::IVirtual
    {
        virtual void* target() const = 0;

        virtual R operator()(Args&&... args) const = 0;
    };

    template <class F, class R, class ...Args>
    class FunctionAdapter
        : public ut::CopyableMovableMixin<IFunction<R, Args...>,
            FunctionAdapter<F, R, Args...>>
    {
    public:
        explicit FunctionAdapter(const F& f)
            : mF(f) { }

        explicit FunctionAdapter(F&& f) _ut_noexcept // noexcept abuse
            : mF(std::move(f)) { }

        FunctionAdapter(const FunctionAdapter& other)
            : mF(other.mF) { }

        FunctionAdapter(FunctionAdapter&& other) _ut_noexcept // noexcept abuse
            : mF(std::move(other.mF)) { }

        void* target() const _ut_noexcept final
        {
            return &mF;
        }

        R operator()(Args&&... args) const final
        {
            return mF(std::forward<Args>(args)...);
        }

    private:
        mutable F mF;
    };

    template <class F, class ...Args>
    class FunctionAdapter<F, void, Args...>
        : public ut::CopyableMovableMixin<IFunction<void, Args...>,
            FunctionAdapter<F, void, Args...>>
    {
    public:
        explicit FunctionAdapter(const F& f)
            : mF(f) { }

        explicit FunctionAdapter(F&& f) _ut_noexcept // noexcept abuse
            : mF(std::move(f)) { }

        FunctionAdapter(const FunctionAdapter& other)
            : mF(other.mF) { }

        FunctionAdapter(FunctionAdapter&& other) _ut_noexcept // noexcept abuse
            : mF(std::move(other.mF)) { }

        void* target() const _ut_noexcept final
        {
            return &mF;
        }

        void operator()(Args&&... args) const final
        {
            mF(std::forward<Args>(args)...);
        }

    private:
        mutable F mF;
    };

    template <class R, class ...Args>
    class FunctionAdapter<R (*)(Args...), R, Args...>
        : public ut::CopyableMovableMixin<IFunction<R, Args...>,
            FunctionAdapter<R (*)(Args...), R, Args...>>
    {
    public:
        using function_ptr_type = R (*)(Args...);

        explicit FunctionAdapter(function_ptr_type f) _ut_noexcept
            : mF(f) { }

        FunctionAdapter(const FunctionAdapter& other)
            : mF(other.mF) { }

        FunctionAdapter(FunctionAdapter&& other) _ut_noexcept
            : mF(other.mF) { }

        void* target() const _ut_noexcept final
        {
            return mF;
        }

        R operator()(Args&&... args) const final
        {
            return mF(std::forward<Args>(args)...);
        }

    private:
        function_ptr_type mF;
    };
}

template <class Signature, size_t Capacity>
class ErasedFunction;

template <class R, class ...Args, size_t Capacity>
class ErasedFunction<R (Args...), Capacity>
{
public:
    static const size_t capacity = Capacity;

    ErasedFunction() _ut_noexcept { }

    ErasedFunction(std::nullptr_t) _ut_noexcept { }

    ErasedFunction(R (*f)(Args...)) _ut_noexcept
        : mActionData(
            ut::TypeInPlaceTag<detail::FunctionAdapter<R (*)(Args...), R, Args...>>(), f)
    {
        if (f == nullptr)
            mActionData.reset();
    }

    template <class F>
    ErasedFunction(F&& f)
        : mActionData(
            ut::TypeInPlaceTag<detail::FunctionAdapter<ut::Unqualified<F>, R, Args...>>(),
            std::forward<F>(f)) { }

    ErasedFunction(const ErasedFunction& other) = default;

    ErasedFunction(ErasedFunction&& other) _ut_noexcept // noexcept abuse
        : mActionData(std::move(other.mActionData)) { }

    ErasedFunction& operator=(const ErasedFunction& other) = default;

    ErasedFunction& operator=(ErasedFunction&& other) _ut_noexcept // noexcept abuse
    {
        mActionData = std::move(other.mActionData);

        return *this;
    }

    ErasedFunction& operator=(std::nullptr_t) _ut_noexcept
    {
        mActionData.reset();

        return *this;
    }

    ErasedFunction& operator=(R (*f)(Args...)) _ut_noexcept
    {
        if (f == nullptr)
            mActionData.reset();
        else
            mActionData = action_data_type(
                ut::TypeInPlaceTag<detail::FunctionAdapter<R (*)(Args...), R, Args...>>(), f);

        return *this;
    }

    template <class F>
    ErasedFunction& operator=(F&& f)
    {
        mActionData = action_data_type(
            ut::TypeInPlaceTag<detail::FunctionAdapter<ut::Unqualified<F>, R, Args...>>(),
            std::forward<F>(f));

        return *this;
    }

    void swap(ErasedFunction& other) _ut_noexcept // noexcept abuse
    {
        mActionData.swap(other.mActionData);
    }

    template <class F>
    const F* target() const _ut_noexcept
    {
        return static_cast<F*>(action().target()); // safe cast if F is original type
    }

    template <class F>
    F* target() _ut_noexcept
    {
        return static_cast<F*>(action().target()); // safe cast if F is original type
    }

    explicit operator bool() const _ut_noexcept
    {
        return (bool) mActionData;
    }

    template <class U = R, ut::EnableIfVoid<U> = nullptr>
    void operator()(Args&&... args) const
    {
        if(!mActionData)
            throw std::logic_error("Trying to call a nil function");

        action()(std::forward<Args>(args)...);
    }

    template <class U = R, ut::DisableIfVoid<U> = nullptr>
    R operator()(Args&&... args) const
    {
        if(!mActionData)
            throw std::logic_error("Trying to call a nil function");

        return action()(std::forward<Args>(args)...);
    }

private:
    using action_data_type = ut::VirtualObjectData<detail::IFunction<R, Args...>,
        ut::RoundUp<ut::virtual_header_size + Capacity, ut::max_align_size>::value,
        ut::max_align_size>;

    const detail::IFunction<R, Args...>& action() const _ut_noexcept
    {
        return *mActionData;
    }

    detail::IFunction<R, Args...>& action() _ut_noexcept
    {
        return *mActionData;
    }

    action_data_type mActionData;
};

template <class Signature, size_t Capacity>
void swap(ErasedFunction<Signature, Capacity>& a,
    ErasedFunction<Signature, Capacity>& b) _ut_noexcept
{
    a.swap(b);
}

template <class Signature, size_t Capacity>
bool operator==(const ErasedFunction<Signature, Capacity>& a, std::nullptr_t) _ut_noexcept
{
    return !a;
}

template <class Signature, size_t Capacity>
bool operator!=(const ErasedFunction<Signature, Capacity>& a, std::nullptr_t) _ut_noexcept
{
    return !!a;
}

template <class Signature, size_t Capacity>
bool operator==(std::nullptr_t, const ErasedFunction<Signature, Capacity>& a) _ut_noexcept
{
    return !a;
}

template <class Signature, size_t Capacity>
bool operator!=(std::nullptr_t, const ErasedFunction<Signature, Capacity>& a) _ut_noexcept
{
    return !!a;
}

static const size_t default_function_capacity = ut::Max<
    5 * ut::ptr_size,
    sizeof(std::function<void ()>)>::value;

// Function type guaranteed to perform small object optimization for
// closures up to default_function_capacity.
//
template <class Signature>
using Function = ErasedFunction<Signature, default_function_capacity>;

}
