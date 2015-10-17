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
#include "TypeTraits.h"
#include <tuple>

namespace ut {

//
// Type evaluation
//

template <class T>
using Id = T;

template <class T>
using Invoke = typename T::type;

//
// Math utils
//

template <int X, int Y>
struct Min
{
    static const int value = ((X < Y) ? X : Y);
};

template <int X, int Y>
struct Max
{
    static const int value = ((X < Y) ? Y : X);
};

template <int N, int Unit>
struct RoundUp
{
    // Round N up to a multiple of Unit
    static const int value = (N + Unit - 1) / Unit * Unit;
};

//
// Int list
//

template <int ...Entries>
struct IntList { };

//
// Int list properties
//

namespace detail
{
    template <int ...Entries>
    struct IntListSizeImpl;

    template <int First, int ...Rest>
    struct IntListSizeImpl<First, Rest...>
    {
        static const size_t value = 1 + IntListSizeImpl<Rest...>::value;
    };

    template <>
    struct IntListSizeImpl<>
    {
        static const size_t value = 0;
    };

    template<size_t I, int ...Entries>
    struct IntListElementImpl;

    template<size_t I, int First, int ...Rest>
    struct IntListElementImpl<I, First, Rest...>
        : IntListElementImpl<I - 1, Rest...>{};

    template<int First, int ...Rest>
    struct IntListElementImpl<0, First, Rest...>
    {
        static const int value = First;
    };

    template<int Value, int ...Entries>
    struct IntListContainsImpl;

    template<int Value, int First, int ...Rest>
    struct IntListContainsImpl<Value, First, Rest...> : BoolConstant<
        Value == First || IntListContainsImpl<Value, Rest...>::value> { };

    template<int Value>
    struct IntListContainsImpl<Value> : std::false_type { };
}

template <class T>
struct IntListSize;

template <int ...Entries>
struct IntListSize<IntList<Entries...>>
    : detail::IntListSizeImpl<Entries...> { };

template <size_t I, class T>
struct IntListElement;

template <size_t I, int ...Entries>
struct IntListElement<I, IntList<Entries...>>
    : detail::IntListElementImpl<I, Entries...> { };

template <int Value, class T>
struct IntListContains;

template <int Value, int ...Entries>
struct IntListContains<Value, IntList<Entries...>>
    : detail::IntListContainsImpl<Value, Entries...> { };

//
// Int list generation
//

namespace detail
{
    template <size_t N, int ...S>
    struct GenIntSeq : GenIntSeq<N - 1, N - 1, S...> { };

    template <int ...S>
    struct GenIntSeq<0, S...>
    {
        using type = IntList<S...>;
    };
}

template <size_t N>
using IntSeq = typename detail::GenIntSeq<N>::type;

//
// Type list properties
//

template <size_t I, class T>
using TupleElement = typename std::tuple_element<I, T>::type;

template <size_t I, class ...Types>
using TypeAt = TupleElement<I, std::tuple<Types...>>;

//
// Logical operators
//

template <class Condition>
using Not = BoolConstant<!Condition::value>;

template <class ...Conditions>
struct All;

template <>
struct All<> : std::true_type { };

template <class First, class ...Rest>
struct All<First, Rest...>
    : Conditional<First::value, All<Rest...>, std::false_type> { };

template <class ...Conditions>
struct Any;

template <>
struct Any<> : std::false_type { };

template <class First, class ...Rest>
struct Any<First, Rest...>
    : Conditional<First::value, std::true_type, Any<Rest...>> { };

template <class ...Conditions>
struct None
    : BoolConstant<!Any<Conditions...>::value> { };

//
// Utils for function overloading
//

struct NoThrowTag;

struct ThrowTag;

namespace detail
{
    template <class T>
    struct SpecializationImpl
    {
        using type = void;
    };

    template <class ...Ts>
    struct VoidImpl
    {
        using type = void;
    };

    template <bool IsNoThrow>
    struct TagByNoThrowImpl
    {
        using type = ThrowTag;
    };

    template <>
    struct TagByNoThrowImpl<true>
    {
        using type = NoThrowTag;
    };
}

struct DelegateTag { };

struct NoThrowTag : DelegateTag { };

struct ThrowTag : DelegateTag { };

template <bool IsNoThrow>
using TagByNoThrow = typename detail::TagByNoThrowImpl<IsNoThrow>::type;

template <class T>
using TagByNoThrowCopyConstructible = TagByNoThrow<std::is_nothrow_copy_constructible<T>::value>;

template <class T>
using TagByNoThrowMoveConstructible = TagByNoThrow<std::is_nothrow_move_constructible<T>::value>;

template <class T>
using TagByNoThrowCopyable = TagByNoThrow<IsNoThrowCopyable<T>::value>;

template <class T>
using TagByNoThrowMovable = TagByNoThrow<IsNoThrowMovable<T>::value>;

template <size_t I>
using Overload = std::integral_constant<int, I>*;

template <class T>
using Specialization = typename detail::SpecializationImpl<T>::type;

template <class ...Ts>
using Void = typename detail::VoidImpl<Ts...>::type;

//
// Type list manipulation
//

namespace detail
{
    template <size_t I, class ...Types>
    struct SkipImpl;

    template<size_t I, class First, class ...Rest>
    struct SkipImpl<I, First, Rest...>
        : SkipImpl<I - 1, Rest...> { };

    template<class ...Rest>
    struct SkipImpl<0, Rest...>
    {
        using type = std::tuple<Rest...>;
    };

    template <size_t I, class T>
    struct TupleSkipImpl;

    template <size_t I, class ...Types>
    struct TupleSkipImpl<I, std::tuple<Types...>>
        : SkipImpl<I, Types...> { };

    template <size_t I, class Accumulator, class ...Types>
    struct TakeImpl;

    template <size_t I, class ...Taken, class First, class ...Rest>
    struct TakeImpl<I, std::tuple<Taken...>, First, Rest...>
        : TakeImpl<I - 1, std::tuple<Taken..., First>, Rest...> { };

    template <class ...Taken, class ...Rest>
    struct TakeImpl<0, std::tuple<Taken...>, Rest...>
    {
        using type = std::tuple<Taken...>;
    };

    template <size_t I, class T>
    struct TupleTakeImpl;

    template <size_t I, class ...Types>
    struct TupleTakeImpl<I, std::tuple<Types...>>
        : TakeImpl<I, std::tuple<>, Types...> { };
}

template <size_t I, class ...Types>
using Skip = typename detail::SkipImpl<I, Types...>::type;

template <size_t I, class T>
using TupleSkip = typename detail::TupleSkipImpl<I, T>::type;

template <size_t I, class ...Types>
using Take = typename detail::TakeImpl<I, std::tuple<>, Types...>::type;

template <size_t I, class T>
using TupleTake = typename detail::TupleTakeImpl<I, T>::type;

//
// Type manipulation
//

namespace detail
{
    template <class T, class From, class To>
    struct ReplaceImpl
    {
        using type = T;
    };

    template <class From, class To>
    struct ReplaceImpl<From, From, To>
    {
        using type = To;
    };
}

template <class T, class From, class To>
using Replace = typename detail::ReplaceImpl<T, From, To>::type;

}
