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
#include "../Task.h"
#include "../util/ContextRef.h"
#include "../util/MoveOnCopy.h"
#include <boost/system/system_error.hpp>

namespace ut { namespace asio {

template <class R>
class PromiseHandlerBase
{
public:
    PromiseHandlerBase(Promise<R>&& promise) _ut_noexcept
        : mPromise(makeMoveOnCopy(std::move(promise))) { }

    template <class U>
    void operator()(const boost::system::error_code& ec, U&& arg) const _ut_noexcept
    {
        Promise<R> promise = mPromise.take();

        if (promise.isCompletable()) {
            if (ec)
                promise.fail(makeExceptionPtr(boost::system::system_error(ec)));
            else
                promise.complete(std::forward<U>(arg));
        }
    }

private:
    DefaultMoveOnCopy<Promise<R>> mPromise;
};

template <>
class PromiseHandlerBase<void>
{
public:
    PromiseHandlerBase(Promise<void>&& promise) _ut_noexcept
        : mPromise(makeMoveOnCopy(std::move(promise))) { }

    void operator()(const boost::system::error_code& ec) const _ut_noexcept
    {
        Promise<void> promise = mPromise.take();

        if (promise.isCompletable()) {
            if (ec)
                promise.fail(makeExceptionPtr(boost::system::system_error(ec)));
            else
                promise.complete();
        }
    }

private:
    DefaultMoveOnCopy<Promise<void>> mPromise;
};

template <class R, class Context = Nothing>
class PromiseHandler;

template <class R>
class PromiseHandler<R, std::shared_ptr<void>> : public PromiseHandlerBase<R>
{
    using base_type = PromiseHandlerBase<R>;

public:
    PromiseHandler(Promise<R>&& promise) _ut_noexcept
        : base_type(std::move(promise)) { }

    template <class U, class Alloc>
    PromiseHandler(Promise<R>&& promise, const ContextRef<U, Alloc>& ctx) _ut_noexcept
        : base_type(std::move(promise))
        , mPtr(ctx.ptr()) { }

private:
    std::shared_ptr<void> mPtr;
};

template <class R>
class PromiseHandler<R, Nothing> : public PromiseHandlerBase<R>
{
    using base_type = PromiseHandlerBase<R>;

public:
    PromiseHandler(Promise<R>&& promise) _ut_noexcept
        : base_type(std::move(promise)) { }
};

template <class R>
PromiseHandler<R> makeHandler(Promise<R>&& promise) _ut_noexcept
{
    return PromiseHandler<R>(std::move(promise));
}

template <class R, class U, class Alloc>
auto makeHandler(Promise<R>&& promise, const ContextRef<U, Alloc>& ctx) _ut_noexcept
    -> PromiseHandler<R, std::shared_ptr<void>>
{
    return PromiseHandler<R, std::shared_ptr<void>>(std::move(promise), ctx);
}

template <class R>
PromiseHandler<R> makeHandler(Task<R>& task) _ut_noexcept
{
    return makeHandler(task.takePromise());
}

template <class R, class U, class Alloc>
auto makeHandler(Task<R>& task, const ContextRef<U, Alloc>& ctx) _ut_noexcept
    -> PromiseHandler<R, std::shared_ptr<void>>
{
    return makeHandler(task.takePromise(), ctx);
}

} } // ut::asio
