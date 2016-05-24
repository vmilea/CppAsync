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
#include "util/TypeTraits.h"

namespace ut {

class Awaiter;
class AwaitableBase;

template <class Awaitable>
struct AwaitableTraits;

namespace detail
{
    template <class C>
    class HasMethod_hasError
    {
        template <class U> static std::true_type test(decltype(&U::hasError));
        template <class U> static std::false_type test(...);

    public:
        using type = decltype(test<C>(nullptr));
        static const bool value = type::value;
    };

    template <class Awaitable>
    struct AwaitableResultImpl
    {
        using type = decltype(AwaitableTraits<Awaitable>::takeResult(
            std::declval<Awaitable&>()));
    };

    namespace awaitable
    {
        template <class Awaitable>
        bool isReady(const Awaitable& awt) _ut_noexcept
        {
            return AwaitableTraits<Awaitable>::isReady(awt);
        }

        template <class Awaitable>
        bool hasError(const Awaitable& awt) _ut_noexcept
        {
            return AwaitableTraits<Awaitable>::hasError(awt);
        }

        template <class Awaitable>
        void setAwaiter(Awaitable& awt, Awaiter *awaiter) _ut_noexcept
        {
            AwaitableTraits<Awaitable>::setAwaiter(awt, awaiter);
        }

        template <class Awaitable>
        auto takeResult(Awaitable& awt)
            -> decltype(AwaitableTraits<Awaitable>::takeResult(awt))
        {
            return AwaitableTraits<Awaitable>::takeResult(awt);
        }

        template <class Awaitable>
        Error takeError(Awaitable& awt) _ut_noexcept
        {
            return AwaitableTraits<Awaitable>::takeError(awt);
        }
    }
}

//
// Awaiter
//

class Awaiter
{
public:
    virtual ~Awaiter() _ut_noexcept { }

    virtual void resume(AwaitableBase *resumer) _ut_noexcept = 0;
};

//
// Awaitable - default shims
//

template <class Awaitable>
bool awaitable_isReady(const Awaitable& awt) _ut_noexcept
{
    return awt.isReady();
}

template <class Awaitable,
    EnableIf<detail::HasMethod_hasError<Awaitable>::value> = nullptr>
bool awaitable_hasError(const Awaitable& awt) _ut_noexcept
{
    return awt.hasError();
}

template <class Awaitable,
    EnableIf<!detail::HasMethod_hasError<Awaitable>::value> = nullptr>
bool awaitable_hasError(const Awaitable& awt) _ut_noexcept
{
    static_assert(!Eval<Awaitable>::value,
        "Awaitable type doesn't support checking for error. "
        "Use ut_await_no_throw_() instead.");

    return false;
}

template <class Awaitable>
void awaitable_setAwaiter(Awaitable& awt, Awaiter *awaiter) _ut_noexcept
{
    awt.setAwaiter(awaiter);
}

template <class Awaitable>
auto awaitable_takeResult(Awaitable& awt)
    -> RemoveReference<decltype(awt.takeResult())>
{
    return awt.takeResult();
}

template <class Awaitable,
    EnableIf<detail::HasMethod_hasError<Awaitable>::value> = nullptr>
Error awaitable_takeError(Awaitable& awt) _ut_noexcept
{
    return awt.takeError();
}

template <class Awaitable,
    EnableIf<!detail::HasMethod_hasError<Awaitable>::value> = nullptr>
Error awaitable_takeError(Awaitable& awt) _ut_noexcept
{
    ut_dcheck(awaitable_hasError(awt));

    return Error();
}

//
// AwaitableTraits
//

template <class Awaitable>
struct AwaitableTraits
{
    static bool isReady(const Awaitable& awt) _ut_noexcept
    {
        return awaitable_isReady(awt);
    }

    static bool hasError(const Awaitable& awt) _ut_noexcept
    {
        return awaitable_hasError(awt);
    }

    static void setAwaiter(Awaitable& awt, Awaiter *awaiter) _ut_noexcept
    {
        awaitable_setAwaiter(awt, awaiter);
    }

    template <class U = Awaitable>
    static auto takeResult(U& awt)
        -> decltype(awaitable_takeResult(awt))
    {
        return awaitable_takeResult(awt);
    }

    static Error takeError(Awaitable& awt) _ut_noexcept
    {
        return awaitable_takeError(awt);
    }
};

template <class Awaitable>
using AwaitableResult = typename detail::AwaitableResultImpl<Awaitable>::type;

}
