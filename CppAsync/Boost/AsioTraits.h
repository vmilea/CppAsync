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

#include "../impl/Common.h"
#include "../Task.h"
#include "../util/ContextRef.h"
#include "../util/MoveOnCopy.h"
#include <boost/asio/async_result.hpp>
#include <boost/asio/handler_type.hpp>
#include <boost/system/system_error.hpp>

namespace ut { namespace asio {

template <class Context>
struct AsTask;

template <>
struct AsTask<std::shared_ptr<void>>
{
    std::shared_ptr<void> ctx;

    AsTask(std::shared_ptr<void> ctx) _ut_noexcept
        : ctx(std::move(ctx)) { }

    AsTask(AsTask&& other) _ut_noexcept
        : ctx(std::move(other.ctx)) { }

    AsTask& operator=(AsTask&& other) _ut_noexcept
    {
        ut_assert(this != &other);

        ctx = std::move(other.ctx);

        return *this;
    }
};

template <>
struct AsTask<Nothing>
{
    AsTask<std::shared_ptr<void>> operator[](
        std::shared_ptr<void> ctx) const _ut_noexcept
    {
        return AsTask<std::shared_ptr<void>>(std::move(ctx));
    }

    template <class U, class Alloc>
    AsTask<std::shared_ptr<void>> operator[](
        const ContextRef<U, Alloc>& ctx) const _ut_noexcept
    {
        return (*this)[ctx.ptr()];
    }
};

#if defined(_MSC_VER) && _MSC_VER < 1900
const AsTask<Nothing> asTask {};
#else
constexpr AsTask<Nothing> asTask {};
#endif

namespace detail
{
    template <class R>
    class AsTaskHandlerBase
    {
    public:
        void initialize(Promise<R>&& promise) _ut_noexcept
        {
            mPromise = makeUncheckedMoveOnCopy(std::move(promise));
        }

        template <class U>
        void operator()(const boost::system::error_code& ec, U&& result) _ut_noexcept
        {
            Promise<R> promise = mPromise.take();

            if (promise.isCompletable()) {
                if (ec)
                    promise.fail(boost::system::system_error(ec));
                else
                    promise.complete(std::forward<U>(result));
            }
        }

    protected:
        AsTaskHandlerBase() _ut_noexcept
            : mPromise(Promise<R>()) { }

    private:
        UncheckedMoveOnCopy<Promise<R>> mPromise;
    };

    template <>
    class AsTaskHandlerBase<void>
    {
    public:
        void initialize(Promise<void>&& promise) _ut_noexcept
        {
            mPromise = makeUncheckedMoveOnCopy(std::move(promise));
        }

        void operator()(const boost::system::error_code& ec) _ut_noexcept
        {
            Promise<void> promise = mPromise.take();

            if (promise.isCompletable()) {
                if (ec)
                    promise.fail(boost::system::system_error(ec));
                else
                    promise.complete();
            }
        }

    protected:
        AsTaskHandlerBase() _ut_noexcept
            : mPromise(Promise<void>()) { }

    private:
        UncheckedMoveOnCopy<Promise<void>> mPromise;
    };

    template <class R, class Context>
    class AsTaskHandler : public AsTaskHandlerBase<R>
    {
    public:
        explicit AsTaskHandler(AsTask<Context> /* tag */) _ut_noexcept { }
    };

    template <class R>
    class AsTaskHandler<R, std::shared_ptr<void>> : public AsTaskHandlerBase<R>
    {
    public:
        explicit AsTaskHandler(AsTask<std::shared_ptr<void>> tag) _ut_noexcept
            : mCtx(std::move(tag.ctx)) { }

    private:
        std::shared_ptr<void> mCtx;
    };
}

} } // ut::asio

namespace boost { namespace asio {

template <class R, class Context>
class async_result<ut::asio::detail::AsTaskHandler<R, Context>>
{
public:
    using type = ut::Task<R>;

    explicit async_result(ut::asio::detail::AsTaskHandler<R, Context>& handler) _ut_noexcept
    {
        handler.initialize(mTask.takePromise());
    }

    ut::Task<R> get() _ut_noexcept
    {
        return std::move(mTask);
    }

private:
    async_result(const async_result& other) = delete;
    async_result& operator=(const async_result& other) = delete;

    ut::Task<R> mTask;
};

template <class Context>
struct handler_type<ut::asio::AsTask<Context>, void(boost::system::error_code)>
{
    using type = ut::asio::detail::AsTaskHandler<void, Context>;
};

template <class R, class Context>
struct handler_type<ut::asio::AsTask<Context>, void(boost::system::error_code, R)>
{
    using type = ut::asio::detail::AsTaskHandler<R, Context>;
};

} } // boost::asio
