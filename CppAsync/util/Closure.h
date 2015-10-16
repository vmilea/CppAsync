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
#include "FunctionTraits.h"
#include "Meta.h"

namespace ut {

enum CopyPolicy
{
    CPY_Default,
    CPY_MoveOnCopy,
    CPY_ThrowOnCopy,
    CPY_Disabled,
};

template <CopyPolicy policy, class F, class ...Captures>
class Closure;

template <class F, class ...Captures>
class ClosureBase
{
public:
    static const bool is_const = FunctionTraits<F>::is_const;
    using args_tuple_type = TupleSkip<sizeof...(Captures), FunctionArgs<F>>;
    using result_type = FunctionResult<F>;

    using captures_tuple_type = std::tuple<Captures...>;
    using lambda_type = F;

    ClosureBase(F&& f, Captures&&... args)
        : mF(std::move(f))
        , mCaptures(std::move(args)...) { }

    ClosureBase(const ClosureBase& other)
        : mF(other.mF)
        , mCaptures(other.mCaptures) { }

    ClosureBase(ClosureBase&& other)
        : mF(std::move(other.mF))
        , mCaptures(std::move(other.mCaptures)) { }

    ClosureBase& operator=(const ClosureBase& other)
    {
        if (this != &other) {
            mF = other.mF;
            mCaptures = other.mCaptures;
        }

        return *this;
    }

    ClosureBase& operator=(ClosureBase&& other)
    {
        ut_assert(this != &other);

        mF = std::move(other.mF);
        mCaptures = std::move(other.mCaptures);

        return *this;
    }

    template <class ...Args>
    result_type operator()(Args&&... args) const
    {
        return call(
            ut::IntSeq<sizeof...(Captures)>(),
            std::forward<Args>(args)...);
    }

    template <class ...Args>
    result_type operator()(Args&&... args)
    {
        return call(
            ut::IntSeq<sizeof...(Captures)>(),
            std::forward<Args>(args)...);
    }

protected:
    template <int ...Indices, class ...Args>
    result_type call(ut::IntList<Indices...>, Args&&... args) const
    {
        return mF(
            std::get<Indices>(mCaptures)...,
            std::forward<Args>(args)...);
    }

    template <int ...Indices, class ...Args>
    result_type call(ut::IntList<Indices...>, Args&&... args)
    {
        return mF(
            std::get<Indices>(mCaptures)...,
            std::forward<Args>(args)...);
    }

private:
    F mF;
    std::tuple<Captures...> mCaptures;
};

template <class F, class ...Captures>
class Closure<CPY_Default, F, Captures...>
    : public ClosureBase<F, Captures...>
{
public:
    Closure(F&& f, Captures&&... args)
        : ClosureBase<F, Captures...>(std::move(f), std::move(args)...) { }

    Closure(const Closure& other) = default;

    Closure(Closure&& other)
        : ClosureBase<F, Captures...>(std::move(other)) { }

    Closure& operator=(const Closure& other) = default;

    Closure& operator=(Closure&& other)
    {
        ut_assert(this != &other);

        ClosureBase<F, Captures...>::operator=(std::move(other));

        return *this;
    }
};

#ifdef NDEBUG

template <class F, class ...Captures>
class Closure<CPY_MoveOnCopy, F, Captures...>
    : public ClosureBase<F, Captures...>
{
public:
    Closure(F&& f, Captures&&... args)
        : ClosureBase<F, Captures...>(std::move(f), std::move(args)...) { }

    Closure(const Closure& other)
        : ClosureBase<F, Captures...>(std::move(const_cast<Closure&>(other))) { }

    Closure(Closure&& other)
        : ClosureBase<F, Captures...>(std::move(other)) { }

    Closure& operator=(const Closure& other)
    {
        if (this != &other)
            operator=(std::move(const_cast<Closure&>(other)));

        return *this;
    }

    Closure& operator=(Closure&& other)
    {
        ut_assert(this != &other);

        ClosureBase<F, Captures...>::operator=(std::move(other));

        return *this;
    }
};

#else

template <class F, class ...Captures>
class Closure<CPY_MoveOnCopy, F, Captures...>
    : public ClosureBase<F, Captures...>
{
public:
    Closure(F&& f, Captures&&... args)
        : ClosureBase<F, Captures...>(std::move(f), std::move(args)...)
        , mIsNil(false) { }

    Closure(const Closure& other)
        : ClosureBase<F, Captures...>(std::move(const_cast<Closure&>(other)))
        , mIsNil(false)
    {
        ut_dcheck(!other.mIsNil &&
            "Illegal move-on-copy construction - source closure already moved");

        const_cast<Closure&>(other).mIsNil = true;
    }

    Closure(Closure&& other)
        : ClosureBase<F, Captures...>(std::move(other))
        , mIsNil(other.mIsNil)
    {
        other.mIsNil = true;
    }

    Closure& operator=(const Closure& other)
    {
        if (this != &other) {
            ut_dcheck(!other.mIsNil &&
                "Illegal move-on-copy assignment - source closure already moved");

            *this = std::move(const_cast<Closure&>(other));
        }

        return *this;
    }

    Closure& operator=(Closure&& other)
    {
        ut_assert(this != &other);

        ClosureBase<F, Captures...>::operator=(std::move(other));
        mIsNil = other.mIsNil;
        other.mIsNil = true;

        return *this;
    }

    template <class ...Args>
    typename ClosureBase<F, Captures...>::result_type operator()(Args&&... args) const
    {
        ut_dcheck(!mIsNil &&
            "Illegal call - closure has been moved");

        return ClosureBase<F, Captures...>::call(
            ut::IntSeq<sizeof...(Captures)>(),
            std::forward<Args>(args)...);
    }

    template <class ...Args>
    typename ClosureBase<F, Captures...>::result_type operator()(Args&&... args)
    {
        ut_dcheck(!mIsNil &&
            "Illegal call - closure has been moved");

        return ClosureBase<F, Captures...>::call(
            ut::IntSeq<sizeof...(Captures)>(),
            std::forward<Args>(args)...);
    }

private:
    bool mIsNil;
};

#endif // NDEBUG

template <class F, class ...Captures>
class Closure<CPY_Disabled, F, Captures...>
    : public ClosureBase<F, Captures...>
{
public:
    Closure(F&& f, Captures&&... args)
        : ClosureBase<F, Captures...>(std::move(f), std::move(args)...) { }

    Closure(Closure&& other)
        : ClosureBase<F, Captures...>(std::move(other)) { }

    Closure& operator=(Closure&& other)
    {
        ut_assert(this != &other);

        ClosureBase<F, Captures...>::operator=(std::move(other));

        return *this;
    }

private:
    Closure(const Closure& other) = delete;
    Closure& operator=(const Closure& other) = delete;
};

template <class F, class ...Captures>
class Closure<CPY_ThrowOnCopy, F, Captures...>
    : public ClosureBase<F, Captures...>
{
public:
    Closure(const Closure& other)
        : ClosureBase<F, Captures...>(std::move(const_cast<Closure&>(other)))
    {
        ut_check(false &&
            "May not copy-construct a Closure with throw-on-copy policy");
    }

    Closure(F&& f, Captures&&... args)
        : ClosureBase<F, Captures...>(std::move(f), std::move(args)...) { }

    Closure& operator=(const Closure& other)
    {
        ut_check(this == &other &&
            "May not copy-assign a Closure with throw-on-copy policy");

        return *this;
    }

    Closure(Closure&& other)
        : ClosureBase<F, Captures...>(std::move(other)) { }

    Closure& operator=(Closure&& other)
    {
        ut_assert(this != &other);

        ClosureBase<F, Captures...>::operator=(std::move(other));

        return *this;
    }
};

//
// FunctionTraits specialization
//

template <CopyPolicy policy, class F, class ...Captures>
struct FunctionTraits<Closure<policy, F, Captures...>>
    : detail::FunctionTraitsTupleImpl<
    Closure<policy, F, Captures...>::is_const,
    typename Closure<policy, F, Captures...>::result_type,
    typename Closure<policy, F, Captures...>::args_tuple_type>{};

//
// Instance generators
//

#define UT_DEF_MAKE_CLOSURE(policy) \
    \
    static_assert(std::is_rvalue_reference<F&&>::value, \
        "Function expected to be an rvalue"); \
    static_assert(All<std::is_rvalue_reference<Args&&>...>::value, \
        "Capture args must be rvalues"); \
    \
    return Closure<policy, F, Args...>(std::move(f), std::move(args)...);

template <class F, class ...Args>
Closure<CPY_Default, Unqualified<F>, Args...> makeClosure(F&& f, Args&&... args)
{
    UT_DEF_MAKE_CLOSURE(CPY_Default)
}

template <class F, class ...Args>
Closure<CPY_MoveOnCopy, Unqualified<F>, Args...> makeMoveOnCopyClosure(F&& f, Args&&... args)
{
    UT_DEF_MAKE_CLOSURE(CPY_MoveOnCopy)
}

template <class F, class ...Args>
Closure<CPY_ThrowOnCopy, Unqualified<F>, Args...> makeThrowOnCopyClosure(F&& f, Args&&... args)
{
    UT_DEF_MAKE_CLOSURE(CPY_ThrowOnCopy)
}

template <class F, class ...Args>
Closure<CPY_Disabled, Unqualified<F>, Args...> makeNoncopyableClosure(F&& f, Args&&... args)
{
    UT_DEF_MAKE_CLOSURE(CPY_Disabled)
}

}
