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

#include "Common.h"
#include "../util/Range.h"
#include "../CommonAwaitable.h"

namespace ut {

namespace detail
{
    namespace ops
    {
        //
        // Predicates
        //

        using Predicate = bool(*)(const AwaitableBase&);

        inline bool isReady(const AwaitableBase& awt) _ut_noexcept
        {
            return awt.isReady();
        }

        inline bool hasError(const AwaitableBase& awt) _ut_noexcept
        {
            return awt.hasError();
        }

        inline bool hasAwaiter(const AwaitableBase& awt) _ut_noexcept
        {
            return awt.awaiter() != nullptr;
        }

        //
        // Range search
        //

        template <Predicate F>
        AwaitableBase* find()
        {
            return nullptr;
        }

        template <Predicate F, class ...Awaitables>
        AwaitableBase* find(AwaitableBase& first, Awaitables&... rest)
        {
            return F(first)
                ? &first
                : find<F>(rest...);
        }

        template <Predicate F, class It>
        It rFind(Range<It> range)
        {
            while (!range.isEmpty()) {
                if (F(selectAwaitable(*range.first)))
                    break;

                ++range.first;
            }
            return range.first;
        }

        template <Predicate F>
        AwaitableBase* findNot()
        {
            return nullptr;
        }

        template <Predicate F, class ...Awaitables>
        AwaitableBase* findNot(AwaitableBase& first, Awaitables&... rest)
        {
            return (!F(first))
                ? &first
                : findNot<F>(rest...);
        }

        template <Predicate F, class It>
        It rFindNot(Range<It> range)
        {
            while (!range.isEmpty()) {
                if (!F(selectAwaitable(*range.first)))
                    break;

                ++range.first;
            }
            return range.first;
        }

        inline bool isAnyOf(AwaitableBase * /* awt */)
        {
            return false;
        }

        template <class ...Awaitables>
        bool isAnyOf(AwaitableBase *awt, AwaitableBase& first, Awaitables&... rest)
        {
            return (awt == &first)
                ? true
                : isAnyOf(awt, rest...);
        }

        template <class It>
        bool rIsAnyOf(AwaitableBase *awt, Range<It> range)
        {
            for (auto& item : range) {
                if (awt == &selectAwaitable(item))
                    return true;
            }
            return false;
        }

        template <class It>
        It rPositionOf(AwaitableBase *awt, Range<It> range)
        {
            while (!range.isEmpty()) {
                if (awt == &selectAwaitable(*range.first))
                    break;

                ++range.first;
            }
            return range.first;
        }

        //
        // Range predicates
        //

        inline bool allValid()
        {
            return true;
        }

        template <class ...Awaitables>
        bool allValid(AwaitableBase& first, Awaitables&... rest)
        {
            return (!first.isValid())
                ? false
                : allValid(rest...);
        }

        template <class It>
        bool rAllValid(Range<It> range)
        {
            for (auto& item : range) {
                if (!selectAwaitable(item).isValid())
                    return false;
            }
            return true;
        }

        template <Predicate F, class ...Awaitables>
        bool all(Awaitables&... awts)
        {
            return findNot<F>(awts...) == nullptr;
        }

        template <Predicate F, class It>
        bool rAll(Range<It> range)
        {
            return rFindNot<F>(range) == range.last;
        }

        template <Predicate F, class ...Awaitables>
        bool none(Awaitables&... awts)
        {
            return find<F>(awts...) == nullptr;
        }

        template <Predicate F, class It>
        bool rNone(Range<It> range)
        {
            return rFind<F>(range) == range.last;
        }

        template <Predicate F, class ...Awaitables>
        bool any(Awaitables&... awts)
        {
            return !none<F>(awts...);
        }

        template <Predicate F, class It>
        bool rAny(Range<It> range)
        {
            return !rNone<F>(range);
        }

        //
        // Range setters
        //

        inline void setAwaiter(Awaiter * /* awaiter */)
        {
        }

        template <class ...Awaitables>
        void setAwaiter(Awaiter *awaiter, AwaitableBase& first, Awaitables&... rest)
        {
            if (!first.isReady())
                first.setAwaiter(awaiter);

            setAwaiter(awaiter, rest...);
        }

        template <class It>
        void rSetAwaiter(Awaiter *awaiter, Range<It> range)
        {
            for (auto& item : range) {
                AwaitableBase& awt = selectAwaitable(item);

                if (!awt.isReady())
                    awt.setAwaiter(awaiter);
            }
        }
    }
}

}
