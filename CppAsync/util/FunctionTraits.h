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
 * @file  FunctionTraits.h
 *
 * Function signature traits
 *
 */

#pragma once

#include "../impl/Common.h"
#include "Meta.h"

namespace ut {

//
// Pattern matching
//

namespace detail
{
    template <bool is_const_, class R, class ArgsTuple>
    struct FunctionTraitsTupleImpl
    {
        static const bool is_const = is_const_;

        static const int arity = std::tuple_size<ArgsTuple>::value;

        using args_tuple_type = ArgsTuple;

        using result_type = R;
    };

    template <bool is_const, class R, class ...Args>
    struct FunctionTraitsImpl
        : FunctionTraitsTupleImpl<is_const, R, std::tuple<Args...>>
    {
        typedef R signature_type(Args...);
    };
}

template <class F>
struct FunctionTraits;

template <class F>
struct FunctionTraits<F&&>
    : FunctionTraits<F> { };

template <class F>
struct FunctionTraits<F&>
    : FunctionTraits<F> { };

template <class F>
struct FunctionTraits<const F>
    : FunctionTraits<F> { };

template <class F>
struct FunctionTraits
    : FunctionTraits<decltype(&F::operator())> { };

template <class C, class R, class ...Args>
struct FunctionTraits<R(C::*)(Args...)>
    : detail::FunctionTraitsImpl<false, R, Args...> { };

template <class C, class R, class ...Args>
struct FunctionTraits<R(C::*)(Args...) const>
    : detail::FunctionTraitsImpl<true, R, Args...> { };

template <class R, class ...Args>
struct FunctionTraits<R(Args...)>
    : detail::FunctionTraitsImpl<false, R, Args...> { };

template <class R, class ...Args>
struct FunctionTraits<R (*)(Args...)>
    : detail::FunctionTraitsImpl<false, R, Args...> { };

//
// Check if type is a functor
//

namespace detail
{
    template <typename T>
    class IsFunctorImpl
    {
        template <class F>
        static std::true_type test(decltype(&F::operator()));

        template <class F>
        static std::false_type test(...);

    public:
        using type = decltype(test<T>(nullptr));
        static const bool value = type::value;
    };
}

template <class T>
struct IsFunctor
    : detail::IsFunctorImpl<T> { };

//
// Result traits
//

template <class F>
using FunctionResult = typename FunctionTraits<F>::result_type;

template <class F, class R>
using FunctionHasResult = std::is_same<FunctionResult<F>, R>;

//
// Argument traits
//

template <class F>
using FunctionArgs = typename FunctionTraits<F>::args_tuple_type;

template <class F, std::size_t I>
using FunctionArg = TupleElement<I, FunctionArgs<F>>;

template <std::size_t First, std::size_t ...Rest>
struct FunctionHasArity
{
    template <class F>
    struct Type : detail::IntListContainsImpl<
        FunctionTraits<F>::arity, First, Rest...> { };
};

template <class F>
struct FunctionIsNullary
    : FunctionHasArity<0>::template Type<F> { };

template <class F>
struct FunctionIsUnary
    : FunctionHasArity<1>::template Type<F> { };

template <class F>
struct FunctionIsBinary
    : FunctionHasArity<2>::template Type<F> { };

template <class F>
struct FunctorIsTernary
    : FunctionHasArity<3>::template Type<F> { };

//template <class F>
//struct FunctionHasArityLessThan2
//    : FunctionHasArity<0, 1>::template Type<F> { };

template <class F>
using FunctionHasArityLessThan2 = FunctionHasArity<0, 1>::template Type<F>;

}
