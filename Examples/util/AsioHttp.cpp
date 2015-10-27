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
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>

using namespace boost::asio;
using namespace ut::asio;

namespace util { namespace asio {

namespace detail
{
    template <class Socket>
    struct HttpGetFrame : ut::AsyncFrame<std::size_t>
    {
        HttpGetFrame(Socket& socket, streambuf& outBuf,
            const std::string& host, const std::string& path,
            bool persistentConnection, bool readAll,
            const ut::ContextRef<void>& ctx)
            : socket(socket)
            , outBuf(outBuf)
            , host(host)
            , path(path)
            , persistentConnection(persistentConnection)
            , readAll(readAll)
            , responseStream(&outBuf)
            , ctx(ctx.template spawn<Context>()) { }

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

            subtask = async_write(socket, ctx->requestBuf, asTask[ctx]);
            ut_await_(subtask);

            // Read response - HTTP status.
            subtask = async_read_until(socket, outBuf, std::string("\r\n"), asTask[ctx]);
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
            subtask = async_read_until(socket, outBuf, std::string("\r\n\r\n"),
                asTask[ctx]);
            ut_await_(subtask);

            {
                // Process headers.
                contentLength = SIZE_MAX;
                std::string header;

                while (std::getline(responseStream, header) && header != "\r") {
                    if (boost::starts_with(header, "Content-Length: ")) {
                        auto l = header.substr(strlen("Content-Length: "));
                        l.resize(l.size() - 1);
                        contentLength = boost::lexical_cast<std::size_t>(l);
                    }
                }
            }

            if (readAll) {
                std::size_t remainingSize = contentLength - outBuf.size();
                subtask = async_read(socket, outBuf, transfer_exactly(remainingSize), asTask[ctx]);
            }
            ut_await_(subtask);

            ut_return(contentLength);
            ut_end();
        }

    private:
        struct Context
        {
            streambuf requestBuf;
        };

        Socket& socket;
        streambuf& outBuf;
        const std::string host;
        const std::string path;
        const bool persistentConnection;
        const bool readAll;
        std::istream responseStream;
        std::size_t contentLength;
        ut::ContextRef<Context> ctx;

        ut::Task<std::size_t> subtask;
    };

    struct HttpDownloadFrame : ut::AsyncFrame<std::size_t>
    {
        HttpDownloadFrame(io_service& io, streambuf& outBuf,
            const std::string& host, const std::string& path,
            const ut::ContextRef<void>& ctx)
            : outBuf(outBuf)
            , host(host)
            , path(path)
            , ctx(ctx.template spawn<Context>(io)) { }

        void operator()()
        {
            ut_begin();

            connectTask = asyncResolveAndConnect(ctx->socket,
                ip::tcp::resolver::query(host, "http"), ctx);
            ut_await_(connectTask);

            downloadTask = asyncHttpGet(ctx->socket, outBuf, host, path, false, ctx);
            ut_await_(downloadTask);

            ut_return(downloadTask.get());
            ut_end();
        }

    private:
        struct Context
        {
            ip::tcp::socket socket;

            Context(io_service& io)
                : socket(io) { }
        };

        streambuf& outBuf;
        const std::string host;
        const std::string path;
        ut::ContextRef<Context> ctx;

        ut::Task<ip::tcp::endpoint> connectTask;
        ut::Task<std::size_t> downloadTask;
    };

    template <>
    ut::Task<std::size_t> asyncHttpGetImpl<ip::tcp::socket>(
        ip::tcp::socket& socket, streambuf& outBuf,
        const std::string& host, const std::string& path,
        bool persistentConnection, bool readAll,
        const ut::ContextRef<void>& ctx)
    {
        using frame_type = detail::HttpGetFrame<ip::tcp::socket>;

        return ut::startAsyncOf<frame_type>(socket, outBuf,
            host, path, persistentConnection, readAll, ctx);
    }

#ifdef HAVE_OPENSSL

    struct HttpsClientConnectFrame : ut::AsyncFrame<ip::tcp::endpoint>
    {
        using socket_type = ssl::stream<ip::tcp::socket>;

        HttpsClientConnectFrame(socket_type& socket,
            const std::string& host,
            const ut::ContextRef<void>& ctx)
            : socket(socket)
            , host(host)
            , ctx(ctx) { }

        void operator()()
        {
            ut_begin();

            connectTask = asyncResolveAndConnect(socket.lowest_layer(),
                ip::tcp::resolver::query(host, "https"), ctx);
            ut_await_(connectTask);

            socket.lowest_layer().set_option(ip::tcp::no_delay(true));

            handshakeTask = socket.async_handshake(socket_type::client, asTask[ctx]);
            ut_await_(handshakeTask);

            ut_return(connectTask.get());
            ut_end();
        }

    private:
        socket_type& socket;
        const std::string host;
        ut::ContextRef<void> ctx;

        ut::Task<ip::tcp::endpoint> connectTask;
        ut::Task<void> handshakeTask;
    };

    struct HttpsDownloadFrame : ut::AsyncFrame<std::size_t>
    {
        HttpsDownloadFrame(io_service& io, streambuf& outBuf,
            ssl::context_base::method sslVersion,
            const std::string& host, const std::string& path,
            const ut::ContextRef<void>& ctx)
            : outBuf(outBuf)
            , host(host)
            , path(path)
            , ctx(ctx.template spawn<Context>(io, sslVersion)) { }

        void operator()()
        {
            ut_begin();

            connectTask = asyncHttpsClientConnect(ctx->socket, host, ctx);
            ut_await_(connectTask);

            downloadTask = asyncHttpGet(ctx->socket, outBuf, host, path, false, ctx);
            ut_await_(downloadTask);

            ut_return(downloadTask.get());
            ut_end();
        }

    private:
        using socket_type = ssl::stream<ip::tcp::socket>;

        struct Context
        {
            ssl::context sslContext;
            socket_type socket;

            Context(io_service& io, ssl::context_base::method sslVersion)
                : sslContext(sslVersion)
                , socket(io, sslContext) { }
        };

        streambuf& outBuf;
        const std::string host;
        const std::string path;
        ut::ContextRef<Context> ctx;

        ut::Task<ip::tcp::endpoint> connectTask;
        ut::Task<std::size_t> downloadTask;
    };

    template <>
    ut::Task<std::size_t> asyncHttpGetImpl<ssl::stream<ip::tcp::socket>>(
        ssl::stream<ip::tcp::socket>& socket, streambuf& outBuf,
        const std::string& host, const std::string& path, bool persistentConnection, bool readAll,
        const ut::ContextRef<void>& ctx)
    {
        using frame_type = detail::HttpGetFrame<ssl::stream<ip::tcp::socket>>;

        return ut::startAsyncOf<frame_type>(socket, outBuf,
            host, path, persistentConnection, readAll, ctx);
    }

#endif
}

ut::Task<std::size_t> asyncHttpDownload(
    io_service& io, streambuf& outBuf,
    const std::string& host, const std::string& path,
    const ut::ContextRef<void>& ctx)
{
    using frame_type = detail::HttpDownloadFrame;

    return ut::startAsyncOf<frame_type>(io, outBuf, host, path, ctx);
}

#ifdef HAVE_OPENSSL

ut::Task<ip::tcp::endpoint> asyncHttpsClientConnect(
    ssl::stream<ip::tcp::socket>& socket,
    const std::string& host,
    const ut::ContextRef<void>& ctx)
{
    using frame_type = detail::HttpsClientConnectFrame;

    return ut::startAsyncOf<frame_type>(socket, host, ctx);
}

ut::Task<std::size_t> asyncHttpsDownload(
    io_service& io, streambuf& outBuf,
    ssl::context_base::method sslVersion,
    const std::string& host, const std::string& path,
    const ut::ContextRef<void>& ctx)
{
    using frame_type = detail::HttpsDownloadFrame;

    return ut::startAsyncOf<frame_type>(io, outBuf, sslVersion, host, path, ctx);
}

#endif // HAVE_OPENSSL

} } // util::asio

#endif // HAVE_BOOST
