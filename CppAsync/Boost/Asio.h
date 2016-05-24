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
#include "AsioTraits.h"
#include "AsioHandler.h"
#include "../StacklessAsync.h"
#include <boost/asio.hpp>
#include <chrono>

namespace ut { namespace asio {

namespace detail
{
    template <class Socket, class Alloc>
    struct ConnectToAnyFrame
        : AsyncFrame<typename Socket::protocol_type::resolver::iterator>
    {
        using iterator_type = typename Socket::protocol_type::resolver::iterator;

        ConnectToAnyFrame(Socket& socket, iterator_type endpoints,
            const ContextRef<void, Alloc>& ctx)
            : socket(socket)
            , it(endpoints)
            , ctx(ctx) { }

        void operator()()
        {
            boost::system::error_code ec;
            ut_begin();

            while (it != iterator_type()) {
                socket.close(ec);
                if (ec)
                    break;

                subtask = socket.async_connect(*it, asTask[ctx]);
                ut_await_no_throw_(subtask);

                if (subtask.hasError()) {
                    try {
                        rethrowException(std::move(subtask.error()));
                    } catch (const boost::system::system_error& e) {
                        ec = e.code();
                        // Try next.
                    }
                } else {
                    ut_return(it);
                }
            }

            throw boost::system::system_error(
                ec ? ec : boost::asio::error::not_found);
            ut_end();
        }

    private:
        Socket& socket;
        iterator_type it;
        ContextRef<void, Alloc> ctx;
        Task<void> subtask;
    };

    template <class Socket, class Alloc>
    struct ResolveAndConnectFrame
        : AsyncFrame<typename Socket::endpoint_type>
    {
        using base_type = AsyncFrame<typename Socket::endpoint_type>;
        using resolver_type = typename Socket::protocol_type::resolver;
        using query_type = typename resolver_type::query;
        using iterator_type = typename resolver_type::iterator;

        ResolveAndConnectFrame(Socket& socket, const query_type& query,
            const ContextRef<void, Alloc>& ctx)
            : base_type(&query)
            , socket(socket)
            , ctx(ctx.template spawn<Context>(socket.get_io_service())) { }

        void operator()()
        {
            ut_begin();

            subtask = ctx->resolver.async_resolve(
                // Temporary query_type instance is still valid.
                *static_cast<const query_type*>(this->arg()), // safe cast
                asTask[ctx]);
            ut_await_(subtask);

            subtask = startAsyncOf<ConnectToAnyFrame<Socket, Alloc>>(
                socket, subtask.get(), ctx);
            ut_await_(subtask);

            ut_return(*subtask.get());
            ut_end();
        }

    private:
        struct Context
        {
            resolver_type resolver;

            Context(boost::asio::io_service& io)
                : resolver(io) { }
        };

        Socket& socket;
        ContextRef<Context, Alloc> ctx;
        Task<iterator_type> subtask;
    };
}

template <class Clock = std::chrono::steady_clock, class U, class Alloc>
Task<void> asyncWait(boost::asio::io_service& io, const typename Clock::duration& delay,
    const ContextRef<U, Alloc>& ctx)
{
    using timer_type = boost::asio::basic_waitable_timer<Clock>;

    auto handle = makeAllocElementPtr<timer_type>(ctx.allocator(), io, delay);
    auto& timer = *handle;
    auto task = makeTaskWithResource(std::move(handle));

    // Timers depend only on io_service, no need to reference context.
    timer.async_wait(makeHandler(task));
    return task;
}

template <class Clock = std::chrono::steady_clock>
Task<void> asyncWait(boost::asio::io_service& io, const typename Clock::duration& delay)
{
    return asyncWait(io, delay, ContextRef<void>());
}

template <class Clock = std::chrono::steady_clock, class U, class Alloc>
Task<void> asyncWaitUntil(boost::asio::io_service& io, const typename Clock::time_point& timePoint,
    const ContextRef<U, Alloc>& ctx)
{
    using timer_type = boost::asio::basic_waitable_timer<Clock>;

    auto handle = makeAllocElementPtr<timer_type>(ctx.allocator(), io, timePoint);
    auto& timer = *handle;
    auto task = makeTaskWithResource(std::move(handle));

    // Timers depend only on io_service, no need to reference context.
    timer.async_wait(makeHandler(task));
    return task;
}

template <class Clock = std::chrono::steady_clock>
Task<void> asyncWaitUntil(boost::asio::io_service& io, const typename Clock::time_point& timePoint)
{
    return asyncWaitUntil(io, timePoint, ContextRef<void>());
}

template <class Socket, class U, class Alloc>
auto asyncConnectToAny(Socket& socket,
    typename Socket::protocol_type::resolver::iterator endpoints,
    const ContextRef<U, Alloc>& ctx)
    -> Task<typename Socket::protocol_type::resolver::iterator>
{
    using frame_type = detail::ConnectToAnyFrame<Socket, Alloc>;

    return startAsyncOf<frame_type>(std::allocator_arg, ctx.allocator(),
        socket, endpoints, ctx);
}

template <class Socket, class U, class Alloc>
auto asyncResolveAndConnect(Socket& socket,
    const typename Socket::protocol_type::resolver::query& query,
    const ContextRef<U, Alloc>& ctx)
    -> Task<typename Socket::endpoint_type>
{
    using frame_type = detail::ResolveAndConnectFrame<Socket, Alloc>;

    return startAsyncOf<frame_type>(std::allocator_arg, ctx.allocator(),
        socket, query, ctx);
}

} } // ut::asio
