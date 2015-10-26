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

#ifdef HAVE_BOOST

#include "AsioHttp.h"
#include <CppAsync/util/StringUtil.h>
#include <CppAsync/util/Optional.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

namespace util {

namespace detail
{
    template <class Socket>
    struct HttpGetFrame : ut::AsyncFrame<size_t>
    {
        HttpGetFrame(Socket& socket, asio::streambuf& outBuf,
            const ut::ContextRef<void>& ctx,
            const std::string& host, const std::string& path,
            bool persistentConnection, bool readAll)
            : socket(socket)
            , outBuf(outBuf)
            , ctx(ctx.template spawn<Context>())
            , host(host)
            , path(path)
            , persistentConnection(persistentConnection)
            , readAll(readAll)
            , responseStream(&outBuf) { }

        void operator()()
        {
            ut_begin();

            if (!socket.lowest_layer().is_open())
                    throw std::runtime_error("Socket not connected");

            // Write HTTP request.
            {
                std::ostream requestStream(&ctx->requestBuf);

                requestStream << "GET " << path << " HTTP/1.1\r\n";
                requestStream << "Host: " << host << "\r\n";
                requestStream << "Accept: */*\r\n";
                if (!persistentConnection)
                    requestStream << "Connection: close\r\n";
                requestStream << "\r\n";
            }

            subtask = ut::asyncWrite(socket, ctx->requestBuf, ctx);
            ut_await_(subtask);

            // Read response - HTTP status.
            subtask = ut::asyncReadUntil(socket, outBuf, ctx, std::string("\r\n"));
            ut_await_(subtask);

            {
                std::string httpVersion;
                int statusCode;
                std::string statusMessage;

                responseStream >> httpVersion >> statusCode;
                std::getline(responseStream, statusMessage);

                if (!responseStream || !boost::starts_with(httpVersion, "HTTP/"))
                    throw std::runtime_error("Invalid HTTP response");
                if (statusCode != 200)
                    throw std::runtime_error(ut::string_printf(
                        "Bad HTTP status: %d", statusCode));
            }

            // Read response headers.
            subtask = asyncReadUntil(socket, outBuf, ctx, std::string("\r\n\r\n"));
            ut_await_(subtask);

            {
                // Process headers.
                contentLength = SIZE_MAX;
                std::string header;

                while (std::getline(responseStream, header) && header != "\r") {
                    if (boost::starts_with(header, "Content-Length: ")) {
                        auto l = header.substr(strlen("Content-Length: "));
                        l.resize(l.size() - 1);
                        contentLength = boost::lexical_cast<size_t>(l);
                    }
                }
            }

            if (readAll) {
                size_t remainingSize = contentLength - outBuf.size();
                subtask = asyncRead(socket, outBuf, ctx,
                    asio::transfer_exactly(remainingSize));
            }
            ut_await_(subtask);

            ut_return(contentLength);
            ut_end();
        }

    private:
        struct Context
        {
            asio::streambuf requestBuf;
        };

        Socket& socket;
        asio::streambuf& outBuf;
        ut::ContextRef<Context> ctx;
        const std::string host;
        const std::string path;
        const bool persistentConnection;
        const bool readAll;
        std::istream responseStream;
        size_t contentLength;
        ut::Task<size_t> subtask;
    };

    struct HttpDownloadFrame : ut::AsyncFrame<size_t>
    {
        HttpDownloadFrame(asio::io_service& io, asio::streambuf& outBuf,
            const ut::ContextRef<void>& ctx,
            const std::string& host, const std::string& path)
            : outBuf(outBuf)
            , ctx(ctx.template spawn<Context>(io))
            , host(host)
            , path(path) { }

        void operator()()
        {
            ut_begin();

            connectTask = ut::asyncResolveAndConnect(ctx->socket, ctx,
                    asio::ip::tcp::resolver::query(host, "http"));
            ut_await_(connectTask);

            downloadTask = asyncHttpGet(ctx->socket, outBuf, ctx, host, path, false);
            ut_await_(downloadTask);

            ut_return(downloadTask.get());
            ut_end();
        }

    private:
        struct Context
        {
            asio::ip::tcp::socket socket;

            Context(asio::io_service& io)
                : socket(io) { }
        };

        asio::streambuf& outBuf;
        ut::ContextRef<Context> ctx;
        const std::string host;
        const std::string path;
        ut::Task<asio::ip::tcp::endpoint> connectTask;
        ut::Task<size_t> downloadTask;
    };

    template <>
    ut::Task<size_t> asyncHttpGetImpl<asio::ip::tcp::socket>(
        asio::ip::tcp::socket& socket, asio::streambuf& outBuf,
        const ut::ContextRef<void>& ctx,
        const std::string& host, const std::string& path, bool persistentConnection, bool readAll)
    {
        using frame_type = detail::HttpGetFrame<asio::ip::tcp::socket>;

        return ut::startAsyncOf<frame_type>(socket, outBuf, ctx,
            host, path, persistentConnection, readAll);
    }

#ifdef HAVE_OPENSSL

    struct HttpsClientConnectFrame : ut::AsyncFrame<asio::ip::tcp::endpoint>
    {
        using socket_type = asio::ssl::stream<asio::ip::tcp::socket>;

        HttpsClientConnectFrame(socket_type& socket,
            const ut::ContextRef<void>& ctx,
            const std::string& host)
            : socket(socket)
            , ctx(ctx)
            , host(host) { }

        void operator()()
        {
            ut_begin();

            connectTask = asyncResolveAndConnect(socket.lowest_layer(), ctx,
                asio::ip::tcp::resolver::query(host, "https"));
            ut_await_(connectTask);

            socket.lowest_layer().set_option(asio::ip::tcp::no_delay(true));

            handshakeTask = asyncHandshake(socket, ctx, socket_type::client);
            ut_await_(handshakeTask);

            ut_return(connectTask.get());
            ut_end();
        }

    private:
        socket_type& socket;
        ut::ContextRef<void> ctx;
        const std::string host;
        ut::Task<asio::ip::tcp::endpoint> connectTask;
        ut::Task<void> handshakeTask;
    };

    struct HttpsDownloadFrame : ut::AsyncFrame<size_t>
    {
        HttpsDownloadFrame(asio::io_service& io, asio::streambuf& outBuf,
            const ut::ContextRef<void>& ctx,
            asio::ssl::context_base::method sslVersion,
            const std::string& host, const std::string& path)
            : outBuf(outBuf)
            , ctx(ctx.template spawn<Context>(io, sslVersion))
            , host(host)
            , path(path) { }

        void operator()()
        {
            ut_begin();

            connectTask = asyncHttpsClientConnect(ctx->socket, ctx, host);
            ut_await_(connectTask);

            downloadTask = asyncHttpGet(ctx->socket, outBuf, ctx, host, path, false);
            ut_await_(downloadTask);

            ut_return(downloadTask.get());
            ut_end();
        }

    private:
        using socket_type = asio::ssl::stream<asio::ip::tcp::socket>;

        struct Context
        {
            asio::ssl::context sslContext;
            socket_type socket;

            Context(asio::io_service& io, asio::ssl::context_base::method sslVersion)
                : sslContext(sslVersion)
                , socket(io, sslContext) { }
        };

        asio::streambuf& outBuf;
        ut::ContextRef<Context> ctx;
        const std::string host;
        const std::string path;
        ut::Task<asio::ip::tcp::endpoint> connectTask;
        ut::Task<size_t> downloadTask;
    };

    template <>
    ut::Task<size_t> asyncHttpGetImpl<asio::ssl::stream<asio::ip::tcp::socket>>(
        asio::ssl::stream<asio::ip::tcp::socket>& socket, asio::streambuf& outBuf,
        const ut::ContextRef<void>& ctx,
        const std::string& host, const std::string& path, bool persistentConnection, bool readAll)
    {
        using frame_type = detail::HttpGetFrame<asio::ssl::stream<asio::ip::tcp::socket>>;

        return ut::startAsyncOf<frame_type>(socket, outBuf, ctx,
            host, path, persistentConnection, readAll);
    }

#endif
}

ut::Task<size_t> asyncHttpDownload(asio::io_service& io, asio::streambuf& outBuf,
    const ut::ContextRef<void>& ctx,
    const std::string& host, const std::string& path)
{
    using frame_type = detail::HttpDownloadFrame;

    return ut::startAsyncOf<frame_type>(io, outBuf, ctx, host, path);
}

#ifdef HAVE_OPENSSL

ut::Task<asio::ip::tcp::endpoint> asyncHttpsClientConnect(
    asio::ssl::stream<asio::ip::tcp::socket>& socket,
    const ut::ContextRef<void>& ctx,
    const std::string& host)
{
    using frame_type = detail::HttpsClientConnectFrame;

    return ut::startAsyncOf<frame_type>(socket, ctx, host);
}

ut::Task<size_t> asyncHttpsDownload(asio::io_service& io, asio::streambuf& outBuf,
    const ut::ContextRef<void>& ctx,
    asio::ssl::context_base::method sslVersion,
    const std::string& host, const std::string& path)
{
    using frame_type = detail::HttpsDownloadFrame;

    return ut::startAsyncOf<frame_type>(io, outBuf, ctx, sslVersion, host, path);
}

ut::Task<void> asyncShutdown(asio::ssl::stream<asio::ip::tcp::socket>& socket,
    const ut::ContextRef<void>& ctx)
{
    ut::Task<void> task;
    socket.async_shutdown(ut::makeAsioHandler(task, ctx));
    return task;
}

#endif // HAVE_OPENSSL

}

#endif // HAVE_BOOST
