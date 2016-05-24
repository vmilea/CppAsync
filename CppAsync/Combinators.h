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
#include "impl/AwaitableOps.h"
#include "util/AllocElementPtr.h"
#include "Task.h"

namespace ut {

namespace detail
{
    template <class R, class It>
    void completeCombinator(Promise<R>&& promise, It pos, It /* last */)
    {
        promise.complete(pos);
    }

    template <class It>
    void completeCombinator(Promise<AwaitableBase*>&& promise, It pos, It last)
    {
        promise.complete(pos == last ? nullptr : *pos);
    }

    //
    // AnyAwaiter
    //

    template <class R, class Container>
    struct AnyAwaiter : Awaiter
    {
        Container awts;
        Promise<R> promise;

        AnyAwaiter(Container&& awts)
            : awts(std::move(awts))
        {
            detail::ops::rSetAwaiter(this, makeRange(this->awts));
        }

        ~AnyAwaiter() _ut_noexcept
        {
            if (promise.state() == PromiseBase::ST_OpCanceled) {
                for (auto& item : makeRange(awts)) {
                    auto& awt = selectAwaitable(item);

                    ut_dcheck(awt.isValid() &&
                        "Awaitables may not be altered while being awaited. Make sure they are "
                        "not being invalidated before whenAny() Task.");
                    ut_dcheck((!awt.isReady() && awt.awaiter() == this) &&
                        "Awaitables may not be altered while being awaited. Make sure to destruct "
                        "or cancel the whenAny() Task in advance.");

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
                    completeCombinator(std::move(promise), pos, range.last);
                    return;
                } else {
                    ut_assert(!awt->isReady() && awt->awaiter() == this);
                    awt->setAwaiter(nullptr);
                }
            }

            ut_assert(false); // Resumer should have been one of the awaited.
        }
    };

    template <class R, class Container, class Alloc>
    Task<R> whenAnyImpl(const Alloc& alloc, Container&& awts)
    {
        using namespace detail::ops;

        static_assert(IsIterable<Container>::value,
                "Container must provide begin() .. end()");

        ut_dcheck(rAllValid(makeRange(awts)) &&
            "Can't combine invalid objects");

        auto range = makeRange(awts);
        auto pos = rFind<isReady>(range);
        if (pos != range.last) {
            Task<R> task;
            completeCombinator(task.takePromise(), pos, range.last);
            return task;
        }

        using awaiter_handle_type = AllocElementPtr<detail::AnyAwaiter<R, Container>, Alloc>;
        using listener_type = detail::BoundResourceListener<R, awaiter_handle_type>;

        awaiter_handle_type handle(alloc, std::move(awts));

#ifdef UT_NO_EXCEPTIONS
        if (handle == nullptr) {
            Task<R> task;
            task.takePromise();
            return task; // Return invalid task.
        }
#endif

        auto task = makeTaskWithListener<listener_type>(std::move(handle));
        auto& awaiter = *task.template listenerAs<listener_type>().resource;
        awaiter.promise = task.takePromise();

        return task;
    }

    //
    // SomeAwaiter
    //

    template <class R, class Container>
    struct SomeAwaiter : Awaiter
    {
        Container awts;
        std::size_t count;
        Promise<R> promise;

        SomeAwaiter(std::size_t count, Container&& awts)
            : awts(std::move(awts))
            , count(count)
        {
            detail::ops::rSetAwaiter(this, makeRange(this->awts));
        }

        ~SomeAwaiter() _ut_noexcept
        {
            if (promise.state() == PromiseBase::ST_OpCanceled) {
                for (auto& item : makeRange(awts)) {
                    auto& awt = selectAwaitable(item);

                    ut_dcheck(awt.isValid() &&
                        "Awaitables may not be altered while being awaited. Make sure they are "
                        "not being invalidated before whenSome/whenAll() Task.");

                    if (!awt.isReady()) {
                        ut_dcheck(awt.awaiter() == this &&
                            "Awaitables may not be altered while being awaited. Make sure to "
                            "destruct or cancel the whenSome/whenAll() Task in advance.");

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
                        completeCombinator(std::move(promise), pos, range.last);
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
                completeCombinator(std::move(promise), range.last, range.last);
            }
        }
    };

    template <class R, class Container, class Alloc>
    Task<R> whenSomeImpl(const Alloc& alloc,
        std::size_t count, Container&& awts)
    {
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
                if (awt.hasError()) {
                    Task<R> task;
                    completeCombinator(task.takePromise(), it, range.last);
                    return task;
                }

                if (count > 0)
                    count--;
            }
        }

        if (count == 0) {
            Task<R> task;
            completeCombinator(task.takePromise(), range.last, range.last);
            return task;
        }

        using awaiter_handle_type = AllocElementPtr<detail::SomeAwaiter<R, Container>, Alloc>;
        using listener_type = detail::BoundResourceListener<R, awaiter_handle_type>;

        awaiter_handle_type handle(alloc, count, std::move(awts));

#ifdef UT_NO_EXCEPTIONS
        if (handle == nullptr) {
            Task<R> task;
            task.takePromise();
            return task; // Return invalid task.
        }
#endif

        auto task = makeTaskWithListener<listener_type>(std::move(handle));
        auto& awaiter = *task.template listenerAs<listener_type>().resource;
        awaiter.promise = task.takePromise();

        return task;
    }
}

//
// whenAny
//

template <class It, class Alloc>
Task<It> whenAny(const Alloc& alloc, Range<It> awts)
{
    return detail::whenAnyImpl<It>(alloc, std::move(awts));
}

template <class It>
Task<It> whenAny(Range<It> awts)
{
    return detail::whenAnyImpl<It>(std::allocator<char>(), std::move(awts));
}

template <class Container, class Alloc,
    EnableIf<IsIterable<Container>::value> = nullptr>
Task<IteratorOf<Container>> whenAny(const Alloc& alloc, Container& awts)
{
    return whenAny(alloc, makeRange(awts));
}

template <class Container,
    EnableIf<IsIterable<Container>::value> = nullptr>
Task<IteratorOf<Container>> whenAny(Container& awts)
{
    return whenAny(makeRange(awts));
}

template <class ...Awaitables, class Alloc,
    EnableIf<!std::is_base_of<AwaitableBase, Alloc>::value> = nullptr>
Task<AwaitableBase*> whenAny(const Alloc& alloc,
    AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    static_assert(None<std::is_const<Awaitables>...>::value,
        "Expecting non-const lvalue references to Awaitables");

    static const std::size_t total = 2 + sizeof...(Awaitables);

    std::array<AwaitableBase*, total> list { { &first, &second, &rest... } };
    return detail::whenAnyImpl<AwaitableBase*>(alloc, std::move(list));
}

template <class ...Awaitables>
Task<AwaitableBase*> whenAny(AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    return whenAny(std::allocator<char>(), first, second, rest...);
}

//
// whenSome
//

template <class It, class Alloc>
Task<It> whenSome(const Alloc& alloc, std::size_t count, Range<It> awts)
{
    return detail::whenSomeImpl<It>(alloc, count, std::move(awts));
}

template <class It>
Task<It> whenSome(std::size_t count, Range<It> awts)
{
    return detail::whenSomeImpl<It>(std::allocator<char>(), count, std::move(awts));
}

template <class Container, class Alloc,
    EnableIf<IsIterable<Container>::value> = nullptr>
Task<IteratorOf<Container>> whenSome(const Alloc& alloc, std::size_t count, Container& awts)
{
    return whenSome(alloc, count, makeRange(awts));
}

template <class Container,
    EnableIf<IsIterable<Container>::value> = nullptr>
Task<IteratorOf<Container>> whenSome(std::size_t count, Container& awts)
{
    return whenSome(count, makeRange(awts));
}

template <class ...Awaitables, class Alloc>
Task<AwaitableBase*> whenSome(const Alloc& alloc, std::size_t count,
    AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    static_assert(None<std::is_const<Awaitables>...>::value,
        "Expecting non-const lvalue references to Awaitables");

    static const std::size_t total = 2 + sizeof...(Awaitables);

    std::array<AwaitableBase*, total> list { { &first, &second, &rest... } };
    return detail::whenSomeImpl<AwaitableBase*>(alloc, count, std::move(list));
}

template <class ...Awaitables>
Task<AwaitableBase*> whenSome(std::size_t count,
    AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    return whenSome(std::allocator<char>(), count, first, second, rest...);
}

//
// whenAll
//

template <class It, class Alloc>
Task<It> whenAll(const Alloc& alloc, Range<It> awts)
{
    return detail::whenSomeImpl<It>(alloc, awts.length(), std::move(awts));
}

template <class It>
Task<It> whenAll(Range<It> awts)
{
    return detail::whenSomeImpl<It>(std::allocator<char>(), awts.length(), std::move(awts));
}

template <class Container, class Alloc,
    EnableIf<IsIterable<Container>::value> = nullptr>
Task<IteratorOf<Container>> whenAll(const Alloc& alloc, Container& awts)
{
    return whenAll(alloc, makeRange(awts));
}

template <class Container,
    EnableIf<IsIterable<Container>::value> = nullptr>
Task<IteratorOf<Container>> whenAll(Container& awts)
{
    return whenAll(makeRange(awts));
}

template <class ...Awaitables, class Alloc,
    EnableIf<!std::is_base_of<AwaitableBase, Alloc>::value> = nullptr>
Task<AwaitableBase*> whenAll(const Alloc& alloc,
    AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    static_assert(None<std::is_const<Awaitables>...>::value,
        "Expecting non-const lvalue references to Awaitables");

    static const std::size_t total = 2 + sizeof...(Awaitables);

    std::array<AwaitableBase*, total> list { { &first, &second, &rest... } };
    return detail::whenSomeImpl<AwaitableBase*>(alloc, total, std::move(list));
}

template <class ...Awaitables>
Task<AwaitableBase*> whenAll(AwaitableBase& first, AwaitableBase& second, Awaitables&... rest)
{
    return whenAll(std::allocator<char>(), first, second, rest...);
}

}
