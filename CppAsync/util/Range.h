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
#include "Meta.h"
#include <iterator>

namespace ut {

namespace detail
{
    namespace container_traits
    {
        using std::begin;
        using std::end;

        template <class C>
        class IsIterableImpl
        {
            // template <class T> static auto test(std::nullptr_t)
            //    -> Eval<decltype(begin(std::declval<T&>()) == end(std::declval<T&>()))>;

            // Oversimplified check supporting MSVC 12.0
            template <class T> static auto test(std::nullptr_t)
                -> Eval<decltype(begin(std::declval<T&>()), end(std::declval<T&>()))>;

            template <class T> static std::false_type test(...);

        public:
            using type = decltype(test<C>(nullptr));
            static const bool value = type::value;
        };

        template <class C>
        struct ContainerTraitsImpl
        {
            static_assert(IsIterableImpl<C>::value,
                "Type must provide begin() .. end()");

            using iterator_type = decltype(begin(std::declval<C&>()));
        };
    }
}

template <class C>
using IsIterable = detail::container_traits::IsIterableImpl<C>;

template <class C>
using IteratorOf = typename detail::container_traits::ContainerTraitsImpl<C>::iterator_type;

template <class It>
struct Range
{
public:
    It first, last;

    Range(It first, It last)
        : first(first)
        , last(last) { }

    Range(const Range& other) = default;

    Range& operator=(const Range& other) = default;

    It begin() const
    {
        return first;
    }

    It end() const
    {
        return last;
    }

    bool isEmpty() const _ut_noexcept
    {
        return first == last;
    }

    size_t length() const _ut_noexcept
    {
        return std::distance(first, last);
    }
};

template <class It>
Range<It> makeRange(It first, It last)
{
    return Range<It>(first, last);
}

template <class Container>
Range<IteratorOf<Container>> makeRange(Container& container)
{
    using std::begin;
    using std::end;

    return makeRange(begin(container), end(container));
}

}
