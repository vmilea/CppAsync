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
#include <memory>
#include <type_traits>

namespace std {

#ifndef UT_DISABLE_EXCEPTIONS

// Workaround - MSVC 12.0 is missing throw() specifier for std::exception_ptr
#if defined(_MSC_VER) && _MSC_VER < 1900

template <>
struct is_nothrow_copy_constructible<exception_ptr>
    : true_type { };

template <>
struct is_nothrow_copy_assignable<exception_ptr>
    : true_type { };

template <>
struct is_nothrow_move_constructible<exception_ptr>
    : true_type { };

template <>
struct is_nothrow_move_assignable<exception_ptr>
    : true_type { };

#endif

#endif // UT_DISABLE_EXCEPTIONS

}

namespace ut {

//
// Constants
//

namespace detail
{
    struct DummyVirtual
    {
        virtual ~DummyVirtual();
    };
}

static const int ptr_size = sizeof(void *);

static const int virtual_header_size = sizeof(detail::DummyVirtual);

static const int align_of_ptr = std::alignment_of<void *>::value;

static const int max_align_size = std::alignment_of<std::max_align_t>::value;

//
// Aliases for standard type traits
//

// Type aliases (const-volatile)

template<class T>
using RemoveCV = typename std::remove_cv<T>::type;

template<class T>
using RemoveConst = typename std::remove_const<T>::type;

template<class T>
using RemoveVolatile = typename std::remove_volatile<T>::type;

template<class T>
using AddCV = typename std::add_cv<T>::type;

template<class T>
using AddConst = typename std::add_const<T>::type;

template<class T>
using AddVolatile = typename std::add_volatile<T>::type;

// Type aliases (references)

template<class T>
using RemoveReference = typename std::remove_reference<T>::type;

template<class T>
using AddLvalueReference = typename std::add_lvalue_reference<T>::type;

template<class T>
using AddRvalueReference = typename std::add_rvalue_reference<T>::type;

// Type aliases (pointers)

template<class T>
using RemovePointer = typename std::remove_pointer<T>::type;

template<class T>
using AddPointer = typename std::add_pointer<T>::type;

// Type aliases (sign modifiers)

template<class T>
using MakeSigned = typename std::make_signed<T>::type;

template<class T>
using MakeUnsigned = typename std::make_unsigned<T>::type;

// Type aliases (arrays)

template<class T>
using RemoveExtent = typename std::remove_extent<T>::type;

template<class T>
using RemoveAllExtents = typename std::remove_all_extents<T>::type;

// Type aliases (misc)

template<size_t Len, size_t Align>
using AlignedStorage = typename std::aligned_storage<Len, Align>::type;

template<size_t Len>
using MaxAlignedStorage = typename std::aligned_storage<Len, max_align_size>::type;

// template<size_t Len, class ...Types>
// using AlignedUnion = typename std::aligned_union<Len, Types...>::type;

template<class T>
using Decay = typename std::decay<T>::type;

template<bool B, class T, class U>
using Conditional = typename std::conditional<B, T, U>::type;

template<class... T>
using CommonType = typename std::common_type<T...>::type;

template<class T>
using UnderlyingType = typename std::underlying_type<T>::type;

template<class T>
using ResultOf = typename std::result_of<T>::type;

//
// Extra aliases
//

template <bool B>
using BoolConstant = std::integral_constant<bool, B>;

template <int N>
using IntConstant = std::integral_constant<int, N>;

template <class Alloc, class T>
using RebindAlloc = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;

template <class T>
using StorageFor = AlignedStorage<sizeof(T), std::alignment_of<T>::value>;

//
// Extra type traits
//

template <class T>
using Unqualified = RemoveCV<RemoveReference<T>>;

template <class T>
struct IsVoid : BoolConstant<
    std::is_same<T, void>::value> { };

template <class T>
struct IsPlainReference : BoolConstant<
    std::is_lvalue_reference<T>::value
    && !std::is_const<RemoveReference<T>>::value> { };

template <class T>
struct IsPlainOrRvalueReference : BoolConstant<
    std::is_rvalue_reference<T>::value
    || IsPlainReference<T>::value> { };

template <class T>
struct IsConstLvalueReference : BoolConstant<
    std::is_lvalue_reference<T>::value
    && std::is_const<RemoveReference<T>>::value> { };

template <class T>
struct IsCopyAndMoveConstructible : BoolConstant<
    std::is_copy_constructible<T>::value
    && std::is_move_constructible<T>::value> { };

template <class T>
struct IsNoThrowCopyable : BoolConstant<
    std::is_nothrow_copy_constructible<T>::value
    && std::is_nothrow_copy_assignable<T>::value> { };

template <class T>
struct IsNoThrowMovable : BoolConstant<
    std::is_nothrow_move_constructible<T>::value
    && std::is_nothrow_move_assignable<T>::value> { };

template <class T>
struct IsAllocatorArg : BoolConstant<
    std::is_same<Unqualified<T>, std::allocator_arg_t>::value> { };

template <class T, template<class...> class Template>
struct IsSpecializationOf
    : std::false_type { };

template <class ...Args, template<class...> class Template>
struct IsSpecializationOf<Template<Args...>, Template>
    : std::true_type { };

template <class T>
struct IsTypeInPlaceTag
    : IsSpecializationOf<T, TypeInPlaceTag> { };

template <class T>
struct IsTuple
    : IsSpecializationOf<T, std::tuple> { };

//
// SFINAE based traits
//

template <class ...Expressions>
struct Eval
    : std::true_type { };

template <class T, class U>
class IsEqualityComparableTo
{
    template <class Y> static auto test(std::nullptr_t)
        -> Eval<decltype(std::declval<Y&>() == std::declval<U&>())>;
    template <class Y> static std::false_type test(...);

public:
    using type = decltype(test<T>(nullptr));
    static const bool value = type::value;
};

template <typename T>
class HasResultType
{
    template <class U> static std::true_type test(typename U::result_type*);
    template <class U> static std::false_type test(...);

public:
    using type = decltype(test<T>(nullptr));
    static const bool value = type::value;
};

//
// EnableIf
//

template <bool B>
using EnableIf = typename std::enable_if<B, void*>::type;

template <class T>
using EnableIfVoid = EnableIf<IsVoid<T>::value>;

template <class T>
using DisableIfVoid = EnableIf<!IsVoid<T>::value>;

template <class T>
using EnableIfNoThrowCopyConstructible = EnableIf<std::is_nothrow_copy_constructible<T>::value>;

template <class T>
using DisableIfNoThrowCopyConstructible = EnableIf<!std::is_nothrow_copy_constructible<T>::value>;

template <class T>
using EnableIfNoThrowMoveConstructible = EnableIf<std::is_nothrow_move_constructible<T>::value>;

template <class T>
using DisableIfNoThrowMoveConstructible = EnableIf<!std::is_nothrow_move_constructible<T>::value>;

template <class T>
using EnableIfNoThrowCopyable = EnableIf<IsNoThrowCopyable<T>::value>;

template <class T>
using DisableIfNoThrowCopyable = EnableIf<!IsNoThrowCopyable<T>::value>;

template <class T>
using EnableIfNoThrowMovable = EnableIf<IsNoThrowMovable<T>::value>;

template <class T>
using DisableIfNoThrowMovable = EnableIf<!IsNoThrowMovable<T>::value>;

}
