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
#include "util/ContextRef.h"
#include "util/MoveOnCopy.h"
#include "StacklessAsync.h"
#include <boost/asio.hpp>
#include <chrono>

namespace ut {

namespace asio = boost::asio;

namespace detail
{
    template <class Socket, class Alloc>
    struct ConnectToAnyFrame
        : AsyncFrame<typename Socket::protocol_type::resolver::iterator>
    {
        using iterator_type = typename Socket::protocol_type::resolver::iterator;

        ConnectToAnyFrame(Socket& socket, const ContextRef<void, Alloc>& ctx,
            iterator_type endpoints)
            : socket(socket)
            , ctx(ctx)
            , it(endpoints) { }

        void operator()()
        {
            boost::system::error_code ec;
            ut_begin();

            while (it != iterator_type()) {
                socket.close(ec);
                if (!!ec)
                    break;

                subtask = asyncConnect(socket, ctx, *it);
                ut_await_no_throw_(subtask);

                if (subtask.hasError()) {
                    try {
                        rethrowException(subtask.error());
                    } catch (const boost::system::system_error& e) {
                        ec = e.code();
                        // try next
                    }
                } else {
                    ut_return(it);
                }
            }

            throw boost::system::system_error(
                !ec ? asio::error::not_found : ec);
            ut_end();
        }

    private:
        Socket& socket;
        ContextRef<void, Alloc> ctx;
        iterator_type it;
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

        ResolveAndConnectFrame(Socket& socket, const ContextRef<void, Alloc>& ctx,
            const query_type& query)
            : base_type(&query)
            , socket(socket)
            , ctx(ctx.template spawn<Context>(socket.get_io_service())) { }

        void operator()()
        {
            ut_begin();

            subtask = asyncResolve(ctx->resolver, ctx,
                // Temporary query_type instance is still valid.
                *static_cast<const query_type*>(this->arg())); // safe cast
            ut_await_(subtask);

            subtask = asyncConnectToAny(socket, ctx, subtask.get());
            ut_await_(subtask);

            ut_return(*subtask.get());
            ut_end();
        }

    private:
        struct Context
        {
            resolver_type resolver;

            Context(asio::io_service& io)
                : resolver(io) { }
        };

        Socket& socket;
        ContextRef<Context, Alloc> ctx;
        Task<iterator_type> subtask;
    };
}

template <class R>
struct AsioHandlerMixin;

template <class R>
class AsioHandler : public AsioHandlerMixin<R>
{
public:
    AsioHandler(Task<R>& task)
        : mPromise(makeMoveOnCopy(task.takePromise())) { }

    template <class T, class Alloc>
    AsioHandler(Task<R>& task, const ContextRef<T, Alloc>& ctx)
        : mPromise(makeMoveOnCopy(task.takePromise()))
        , mPtr(ctx.ptr()) { }

private:
    using mv_promise_type = decltype(makeMoveOnCopy(std::declval<Promise<R>>()));

    mv_promise_type mPromise;
    std::shared_ptr<void> mPtr;

    friend struct AsioHandlerMixin<R>;
};

template <class R>
struct AsioHandlerMixin
{
    template <class U>
    void operator()(const boost::system::error_code& ec, U&& arg) const
    {
        auto& thiz = static_cast<const AsioHandler<R>&>(*this); // safe cast
        auto promise = thiz.mPromise.take();

        if (promise.isCompletable())
        {
            if (ec)
                promise.fail(makeExceptionPtr(boost::system::system_error(ec)));
            else
                promise(std::forward<U>(arg));
        }
    }
};

template <>
struct AsioHandlerMixin<void>
{
    void operator()(const boost::system::error_code& ec) const
    {
        auto& thiz = static_cast<const AsioHandler<void>&>(*this); // safe cast
        auto promise = thiz.mPromise.take();

        if (promise.isCompletable())
        {
            if (ec)
                promise.fail(makeExceptionPtr(boost::system::system_error(ec)));
            else
                promise();
        }
    }
};

template <class R = void>
AsioHandler<R> makeAsioHandler(Task<R>& task)
{
    return AsioHandler<R>(task);
}

template <class R = void, class RefT, class Alloc>
AsioHandler<R> makeAsioHandler(Task<R>& task, const ContextRef<RefT, Alloc>& ctx)
{
    return AsioHandler<R>(task, ctx);
}

template <class Timer, class RefT, class Alloc>
Task<void> asyncWait(Timer& timer, const ContextRef<RefT, Alloc>& ctx)
{
    Task<void> task;
    timer.async_wait(makeAsioHandler(task, ctx));
    return task;
}

template <class Clock = std::chrono::steady_clock, class RefT, class Alloc>
Task<void> asyncWait(asio::io_service& io, const ContextRef<RefT, Alloc>& ctx,
    const typename Clock::duration& delay)
{
    using timer_type = asio::basic_waitable_timer<Clock>;

    auto handle = makeAllocElementPtr<timer_type>(ctx.allocator(), io, delay);
    auto& timer = *handle;
    auto task = makeTaskWithResource(std::move(handle));

    // Timers depend only on io_service, no need to reference context.
    timer.async_wait(makeAsioHandler(task));
    return task;
}

template <class Clock = std::chrono::steady_clock, class RefT, class Alloc>
Task<void> asyncWaitUntil(asio::io_service& io, const ContextRef<RefT, Alloc>& ctx,
    const typename Clock::time_point& timePoint)
{
    using timer_type = asio::basic_waitable_timer<Clock>;

    auto handle = makeAllocElementPtr<timer_type>(ctx.allocator(), io, timePoint);
    auto& timer = *handle;
    auto task = makeTaskWithResource(std::move(handle));

    // Timers depend only on io_service, no need to reference context.
    timer.async_wait(makeAsioHandler(task));
    return task;
}

template <class Resolver, class RefT, class Alloc>
auto asyncResolve(Resolver& resolver, const ContextRef<RefT, Alloc>& ctx,
    const typename Resolver::query& query)
    -> Task<typename Resolver::iterator>
{
    Task<typename Resolver::iterator> task;
    resolver.async_resolve(query, makeAsioHandler(task, ctx));
    return task;
}

template <class Socket, class RefT, class Alloc>
Task<void> asyncConnect(Socket& socket, const ContextRef<RefT, Alloc>& ctx,
    const typename Socket::endpoint_type& endpoint)
{
    Task<void> task;
    socket.async_connect(endpoint, makeAsioHandler(task, ctx));
    return task;
}

template <class Socket, class RefT, class Alloc>
auto asyncConnectToAny(Socket& socket, const ContextRef<RefT, Alloc>& ctx,
    typename Socket::protocol_type::resolver::iterator endpoints)
    -> Task<typename Socket::protocol_type::resolver::iterator>
{
    using frame_type = detail::ConnectToAnyFrame<Socket, Alloc>;

    return ut::startAsyncOf<frame_type>(std::allocator_arg, ctx.allocator(),
        socket, ctx, endpoints);
}

template <class Socket, class RefT, class Alloc>
auto asyncResolveAndConnect(Socket& socket, const ContextRef<RefT, Alloc>& ctx,
    const typename Socket::protocol_type::resolver::query& query)
    -> Task<typename Socket::endpoint_type>
{
    using frame_type = detail::ResolveAndConnectFrame<Socket, Alloc>;

    return ut::startAsyncOf<frame_type>(std::allocator_arg, ctx.allocator(),
        socket, ctx, query);
}

template <class Acceptor, class PeerSocket, class RefT, class Alloc>
Task<void> asyncAccept(Acceptor& acceptor, PeerSocket& peer, const ContextRef<RefT, Alloc>& ctx)
{
    Task<void> task;
    acceptor.async_accept(peer, makeAsioHandler(task, ctx));
    return task;
}

template <class Acceptor, class PeerSocket, class RefT, class Alloc>
Task<void> asyncAccept(Acceptor& acceptor, PeerSocket& peer,
    typename Acceptor::endpoint_type& peerEndpoint, const ContextRef<RefT, Alloc>& ctx)
{
    Task<void> task;
    acceptor.async_accept(peer, peerEndpoint, makeAsioHandler(task, ctx));
    return task;
}

template <class AsyncWriteStream, class ConstBufferSequence, class CompletionCondition,
    class RefT, class Alloc>
Task<size_t> asyncWrite(AsyncWriteStream& stream,
    const ConstBufferSequence& buffers, const ContextRef<RefT, Alloc>& ctx,
    CompletionCondition completionCondition)
{
    Task<size_t> task;
    asio::async_write(stream, buffers, completionCondition, makeAsioHandler(task, ctx));
    return task;
}

template <class AsyncWriteStream, class ConstBufferSequence, class RefT, class Alloc>
Task<size_t> asyncWrite(AsyncWriteStream& stream,
    const ConstBufferSequence& buffers, const ContextRef<RefT, Alloc>& ctx)
{
    return asyncWrite(stream, buffers, ctx, asio::transfer_all());
}

template <class AsyncWriteStream, class BufferAlloc, class CompletionCondition,
    class RefT, class Alloc>
Task<size_t> asyncWrite(AsyncWriteStream& stream,
    asio::basic_streambuf<BufferAlloc>& buffer, const ContextRef<RefT, Alloc>& ctx,
    CompletionCondition completionCondition)
{
    Task<size_t> task;
    asio::async_write(stream, buffer, completionCondition, makeAsioHandler(task, ctx));
    return task;
}

template <class AsyncWriteStream, class BufferAlloc, class RefT, class Alloc>
Task<size_t> asyncWrite(AsyncWriteStream& stream,
    asio::basic_streambuf<BufferAlloc>& buffer, const ContextRef<RefT, Alloc>& ctx)
{
    return asyncWrite(stream, buffer, ctx, asio::transfer_all());
}

template <class AsyncReadStream, class MutableBufferSequence, class CompletionCondition,
    class RefT, class Alloc>
Task<size_t> asyncRead(AsyncReadStream& stream,
    const MutableBufferSequence& outBuffers, const ContextRef<RefT, Alloc>& ctx,
    CompletionCondition completionCondition)
{
    Task<size_t> task;
    asio::async_read(stream, outBuffers, completionCondition, makeAsioHandler(task, ctx));
    return task;
}

template <class AsyncReadStream, class MutableBufferSequence, class RefT, class Alloc>
Task<size_t> asyncRead(AsyncReadStream& stream,
    const MutableBufferSequence& outBuffers, const ContextRef<RefT, Alloc>& ctx)
{
    return asyncRead(stream, outBuffers, ctx, asio::transfer_all());
}

template <class AsyncReadStream, class BufferAlloc, class CompletionCondition,
    class RefT, class Alloc>
Task<size_t> asyncRead(AsyncReadStream& stream,
    asio::basic_streambuf<BufferAlloc>& outBuffer, const ContextRef<RefT, Alloc>& ctx,
    CompletionCondition completionCondition)
{
    Task<size_t> task;
    asio::async_read(stream, outBuffer, completionCondition, makeAsioHandler(task, ctx));
    return task;
}

template <class AsyncReadStream, class BufferAlloc, class RefT, class Alloc>
Task<size_t> asyncRead(AsyncReadStream& stream,
    asio::basic_streambuf<BufferAlloc>& outBuffer, const ContextRef<RefT, Alloc>& ctx)
{
    return asyncRead(stream, outBuffer, ctx, asio::transfer_all());
}

template <class AsyncReadStream, class BufferAlloc, class RefT, class Alloc>
Task<size_t> asyncReadUntil(AsyncReadStream& stream,
    asio::basic_streambuf<BufferAlloc>& outBuffer, const ContextRef<RefT, Alloc>& ctx,
    char delim)
{
    Task<size_t> task;
    asio::async_read_until(stream, outBuffer, delim, makeAsioHandler(task, ctx));
    return task;
}

template <class AsyncReadStream, class BufferAlloc, class RefT, class Alloc>
Task<size_t> asyncReadUntil(AsyncReadStream& stream,
    asio::basic_streambuf<BufferAlloc>& outBuffer, const ContextRef<RefT, Alloc>& ctx,
    const std::string& delim)
{
    Task<size_t> task;
    asio::async_read_until(stream, outBuffer, delim, makeAsioHandler(task, ctx));
    return task;
}

template <class AsyncReadStream, class BufferAlloc, class RefT, class Alloc>
Task<size_t> asyncReadUntil(AsyncReadStream& stream,
    asio::basic_streambuf<BufferAlloc>& outBuffer, const ContextRef<RefT, Alloc>& ctx,
    const boost::regex& expr)
{
    Task<size_t> task;
    asio::async_read_until(stream, outBuffer, expr, makeAsioHandler(task, ctx));
    return task;
}

template <class AsyncReadStream, class BufferAlloc, class MatchCondition, class RefT, class Alloc,
    EnableIf<asio::is_match_condition<MatchCondition>::value> = nullptr>
Task<size_t> asyncReadUntil(AsyncReadStream& stream,
    asio::basic_streambuf<BufferAlloc>& outBuffer, const ContextRef<RefT, Alloc>& ctx,
    MatchCondition matchCondition)
{
    Task<size_t> task;
    asio::async_read_until(stream, outBuffer, std::move(matchCondition),
        makeAsioHandler(task, ctx));
    return task;
}

}
