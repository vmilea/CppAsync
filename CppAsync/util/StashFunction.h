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
#include "Cast.h"
#include "FunctionTraits.h"
#include "Meta.h"

namespace ut {

template <class Stash>
class StashFunctionBase
{
public:
    template <class ...StashArgs>
    StashFunctionBase(StashArgs&&... args)
    {
        new (&mStashData) Stash(std::forward<StashArgs>(args)...);
    }

    StashFunctionBase(StashFunctionBase&& other)
    {
        new (&mStashData) Stash(std::move(other.stash()));
    }

    StashFunctionBase& operator=(StashFunctionBase&& other)
    {
        ut_assert(this != &other);

        stash() = std::move(other.stash());

        return *this;
    }

    ~StashFunctionBase() _ut_noexcept
    {
        stash().~Stash();
    }

    const Stash& stash() const _ut_noexcept
    {
        return ptrCast<const Stash&>(mStashData); // safe cast
    }

    Stash& stash() _ut_noexcept
    {
        return ptrCast<Stash&>(mStashData); // safe cast
    }

private:
    StashFunctionBase(const StashFunctionBase& other) = delete;
    StashFunctionBase& operator=(const StashFunctionBase& other) = delete;

    MaxAlignedStorage<sizeof(Stash)> mStashData;
};

template <class Stash, class F>
class StashFunction : public StashFunctionBase<Stash>
{
    using base_type = StashFunctionBase<Stash>;

public:
    template <class ...StashArgs>
    StashFunction(F&& f, StashArgs&&... args)
        : base_type(std::forward<StashArgs>(args)...)
        , mF(std::move(f)) { }

    StashFunction(StashFunction&& other)
        : base_type(static_cast<base_type&&>(other)) // safe cast
        , mF(std::move(other.mF)) { }

    StashFunction& operator=(StashFunction&& other)
    {
        base_type::operator=(std::move(other));
        mF = std::move(other.mF);

        return *this;
    }

    const F& function() const _ut_noexcept
    {
        return mF;
    }

    F& function() _ut_noexcept
    {
        return mF;
    }

    template <class ...Args>
    FunctionResult<F> operator()(Args&&... args) const
    {
        return mF(std::forward<Args>(args)...);
    }

    template <class ...Args>
    FunctionResult<F> operator()(Args&&... args)
    {
        return mF(std::forward<Args>(args)...);
    }

private:
    F mF;
};

//
// FunctionTraits specialization
//

template <class Stash, class F>
struct FunctionTraits<StashFunction<Stash, F>>
    : detail::FunctionTraitsTupleImpl<
        FunctionTraits<F>::is_const, FunctionResult<F>, FunctionArgs<F>> { };

//
// Instance generators
//

template <class Stash, class F, class ...StashArgs>
StashFunction<Stash, F> makeStashFunction(F&& f, StashArgs&&... args)
{
    static_assert(std::is_rvalue_reference<F&&>::value,
        "Function expected to be an rvalue");

    return StashFunction<Stash, F>(std::move(f), std::forward<StashArgs>(args)...);
}

}
