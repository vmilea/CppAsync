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

#include "impl/Common.h"
#include "impl/AwaitableOps.h"
#include "util/Instance.h"
#include "util/AllocElementPtr.h"
#include "Task.h"

namespace ut {

namespace detail
{
    //
    // AnyAwaiter
    //

    template <class Container>
    struct AnyAwaiter : Awaiter
    {
        using iterator_type = IteratorOf<Container>;

        Container awts;
        Instance<Promise<iterator_type>> promise;

        AnyAwaiter(Container&& awts)
            : awts(std::move(awts))
        {
            detail::ops::rSetAwaiter(this, makeRange(this->awts));
        }

        ~AnyAwaiter() _ut_noexcept
        {
            if (promise->state() == PromiseBase::ST_OpCanceled) {
                for (auto& item : makeRange(awts)) {
                    auto& awt = selectAwaitable(item);

                    ut_assert(!awt.isReady() && awt.awaiter() == this);
                    awt.setAwaiter(nullptr);
                }
            }
        }

        void resume(AwaitableBase *resumer) _ut_noexcept final
        {
            using namespace detail::ops;

            auto range = makeRange(awts);

            ut_assert(resumer->isReady());

            for (auto it = range.first; it != range.last; ++it) {
                AwaitableBase *awt = &selectAwaitable(*it);

                if (awt == resumer) {
                    ut_assert(awt->isReady() && awt->awaiter() == nullptr);

                    auto pos(it); // Iterator might not be copy assignable, hence inner loop.
                    while (++it != range.last) {
                        awt = &selectAwaitable(*it);
                        ut_assert(!awt->isReady() && awt->awaiter() == this);
                        awt->setAwaiter(nullptr);
                    }
                    promise->complete(pos);
                    return;
                } else {
                    ut_assert(!awt->isReady() && awt->awaiter() == this);
                    awt->setAwaiter(nullptr);
                }
            }

            ut_assert(false); // Resumer should have been one of the awaited.
        }
    };

    template <class Container, class Allocator>
    Task<IteratorOf<Container>> whenAnyImpl(const Allocator& allocator, Container&& awts)
    {
        using iterator_type = IteratorOf<Container>;

        using namespace detail::ops;

        static_assert(IsIterable<Container>::value,
                "Container must provide begin() .. end()");

        ut_dcheck(rAllValid(makeRange(awts)) &&
            "Can't combine invalid objects");

        auto range = makeRange(awts);
        auto pos = rFind<isReady>(range);
        if (pos != range.last)
            return makeCompletedTask<iterator_type>(pos);

        using awaiter_handle_type = AllocElementPtr<detail::AnyAwaiter<Container>, Allocator>;
        using listener_type = detail::BoundResourceListener<iterator_type, awaiter_handle_type>;

        awaiter_handle_type handle(allocator, std::move(awts));

#ifdef UT_DISABLE_EXCEPTIONS
        if (handle.isNil())
            return Task<iterator_type>(); // return nil task
#endif

        auto task = makeTaskWithListener<listener_type>(std::move(handle));
        auto& awaiter = *task.template listenerAs<listener_type>().resource;
        awaiter.promise.initialize(task.takePromise());

        return task;
    }

    //
    // SomeAwaiter
    //

    template <class Container>
    struct SomeAwaiter : Awaiter
    {
        using iterator_type = IteratorOf<Container>;

        Container awts;
        size_t count;
        Instance<Promise<iterator_type>> promise;

        SomeAwaiter(size_t count, Container&& awts)
            : awts(std::move(awts))
            , count(count)
        {
            detail::ops::rSetAwaiter(this, makeRange(this->awts));
        }

        ~SomeAwaiter() _ut_noexcept
        {
            if (promise->state() == PromiseBase::ST_OpCanceled) {
                for (auto& item : makeRange(awts)) {
                    auto& awt = selectAwaitable(item);

                    if (!awt.isReady()) {
                        ut_assert(awt.awaiter() == this);
                        awt.setAwaiter(nullptr);
                    }
                }
            }
        }

        void resume(AwaitableBase *resumer) _ut_noexcept final
        {
            using namespace detail::ops;

            ut_assert(count > 0);

            auto range = makeRange(awts);

            ut_assert(rIsAnyOf(resumer, range));
            ut_assert(resumer->isReady() && resumer->awaiter() == nullptr);
            ut_assert(resumer->hasError() || !rAny<hasError>(range));

            if (resumer->hasError()) {
                for (auto it = range.first; it != range.last; ++it) {
                    AwaitableBase *awt = &selectAwaitable(*it);

                    if (awt == resumer) {
                        auto pos(it); // Iterator might not be copy assignable, hence inner loop.
                        while (++it != range.last) {
                            awt = &selectAwaitable(*it);
                            if (!awt->isReady()) {
                                ut_assert(awt->awaiter() == this);
                                awt->setAwaiter(nullptr);
                            }
                        }
                        promise->complete(pos);
                        return;
                    }

                    if (!awt->isReady()) {
                        ut_assert(awt->awaiter() == this);
                        awt->setAwaiter(nullptr);
                    }
                }

                ut_assert(false); // Resumer should have been one of the awaited.
            } else if (--count == 0) {
                for (auto it = range.first; it != range.last; ++it) {
                    AwaitableBase& awt = selectAwaitable(*it);

                    if (!awt.isReady()) {
                        ut_assert(awt.awaiter() == this);
                        awt.setAwaiter(nullptr);
                    }
                }
                promise->complete(range.last);
            }
        }
    };

    template <class Container, class Allocator>
    Task<IteratorOf<Container>> whenSomeImpl(const Allocator& allocator,
        size_t count, Container&& awts)
    {
        using iterator_type = IteratorOf<Container>;

        using namespace detail::ops;

        static_assert(std::is_rvalue_reference<Container&&>::value, "");

        static_assert(IsIterable<Container>::value,
            "Container must provide begin() .. end()");

        ut_dcheck(rAllValid(makeRange(awts)) &&
            "Can't combine invalid objects");

        auto range = makeRange(awts);

        for (auto it = range.first; it != range.last; ++it) {
            AwaitableBase& awt = selectAwaitable(*it);

            if (awt.isReady()) {
                if (awt.hasError())
                    return makeCompletedTask<iterator_type>(it);

                if (count > 0)
                    count--;
            }
        }

        if (count == 0)
            return makeCompletedTask<iterator_type>(range.last);

        using awaiter_handle_type = AllocElementPtr<detail::SomeAwaiter<Container>, Allocator>;
        using listener_type = detail::BoundResourceListener<iterator_type, awaiter_handle_type>;

        awaiter_handle_type handle(allocator, count, std::move(awts));

#ifdef UT_DISABLE_EXCEPTIONS
        if (handle.isNil())
            return Task<iterator_type>(); // return nil task
#endif

        auto task = makeTaskWithListener<listener_type>(std::move(handle));
        auto& awaiter = *task.template listenerAs<listener_type>().resource;
        awaiter.promise.initialize(task.takePromise());

        return task;
    }
}

//
// whenAny
//

template <class It, class Allocator>
Task<It> whenAny(const Allocator& allocator, Range<It> awts)
{
    return detail::whenAnyImpl(allocator, std::move(awts));
}

template <class It>
Task<It> whenAny(Range<It> awts)
{
    return detail::whenAnyImpl(std::allocator<char>(), std::move(awts));
}

template <class Container, class Allocator>
Task<IteratorOf<Container>> whenAny(const Allocator& allocator, Container& awts)
{
    return whenAny(allocator, makeRange(awts));
}

template <class Container>
Task<IteratorOf<Container>> whenAny(Container& awts)
{
    return whenAny(makeRange(awts));
}

template <class ...Awaitables, class Allocator,
    EnableIf<!std::is_base_of<AwaitableBase, Allocator>::value> = nullptr>
Task<AwaitableBase*> whenAny(const Allocator& allocator,
    AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    static_assert(None<std::is_const<Awaitables>...>::value,
        "Expecting non-const lvalue references to Awaitables");

    static const size_t count = 2 + sizeof...(Awaitables);

    std::array<AwaitableBase*, count> list { { &first, &second, &rest... } };
    return detail::whenAnyImpl(allocator, std::move(list));
}

template <class ...Awaitables>
Task<AwaitableBase*> whenAny(AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    static_assert(None<std::is_const<Awaitables>...>::value,
        "Expecting non-const lvalue references to Awaitables");

    return whenAny(std::allocator<char>(), first, second, rest...);
}

//
// whenSome
//

template <class It, class Allocator>
Task<It> whenSome(const Allocator& allocator, size_t count, Range<It> awts)
{
    return detail::whenSomeImpl(allocator, count, std::move(awts));
}

template <class It>
Task<It> whenSome(size_t count, Range<It> awts)
{
    return detail::whenSomeImpl(std::allocator<char>(), count, std::move(awts));
}

template <class Container, class Allocator>
Task<IteratorOf<Container>> whenSome(const Allocator& allocator, size_t count, Container& awts)
{
    return whenSome(allocator, count, makeRange(awts));
}

template <class Container>
Task<IteratorOf<Container>> whenSome(size_t count, Container& awts)
{
    return whenSome(count, makeRange(awts));
}

//
// whenAll
//

template <class It, class Allocator>
Task<It> whenAll(const Allocator& allocator, Range<It> awts)
{
    return detail::whenSomeImpl(allocator, awts.length(), std::move(awts));
}

template <class It>
Task<It> whenAll(Range<It> awts)
{
    return detail::whenSomeImpl(std::allocator<char>(), awts.length(), std::move(awts));
}

template <class Container, class Allocator>
Task<IteratorOf<Container>> whenAll(const Allocator& allocator, Container& awts)
{
    return whenAll(allocator, makeRange(awts));
}

template <class Container>
Task<IteratorOf<Container>> whenAll(Container& awts)
{
    return whenAll(makeRange(awts));
}

template <class ...Awaitables, class Allocator,
    EnableIf<!std::is_base_of<AwaitableBase, Allocator>::value> = nullptr>
Task<AwaitableBase*> whenAll(const Allocator& allocator,
    AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    static_assert(None<std::is_const<Awaitables>...>::value,
        "Expecting non-const lvalue references to Awaitables");

    static const size_t count = 2 + sizeof...(Awaitables);

    std::array<AwaitableBase*, count> list { { &first, &second, &rest... } };
    return whenSome(allocator, count, list);
}

template <class ...Awaitables>
Task<AwaitableBase*> whenAll(AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    static_assert(None<std::is_const<Awaitables>...>::value,
        "Expecting non-const lvalue references to Awaitables");

    return whenAll(std::allocator<char>(), first, second, rest...);
}

}
