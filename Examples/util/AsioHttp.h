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

#ifdef HAVE_BOOST

#include "../Common.h"
#include <CppAsync/AsioWrappers.h>
#include <string>

#ifdef HAVE_OPENSSL
#include <boost/asio/ssl.hpp>
#endif

namespace util {

namespace asio = boost::asio;

namespace detail
{
    template <class Socket>
    ut::Task<size_t> asyncHttpGetImpl(
        Socket& socket, asio::streambuf& outBuf,
        const ut::ContextRef<void>& ctx,
        const std::string& host, const std::string& path,
        bool persistentConnection, bool readAll);
}

template <class Socket>
ut::Task<size_t> asyncHttpGetHeader(Socket& socket, asio::streambuf& outBuf,
    const ut::ContextRef<void>& ctx,
    const std::string& host, const std::string& path, bool persistentConnection)
{
    return detail::asyncHttpGetImpl(socket, outBuf, ctx,
        host, path, persistentConnection, false);
}

template <class Socket>
ut::Task<size_t> asyncHttpGet(Socket& socket, asio::streambuf& outBuf,
    const ut::ContextRef<void>& ctx,
    const std::string& host, const std::string& path, bool persistentConnection)
{
    return detail::asyncHttpGetImpl(socket, outBuf, ctx,
        host, path, persistentConnection, true);
}

ut::Task<size_t> asyncHttpDownload(asio::io_service& io, asio::streambuf& outBuf,
    const ut::ContextRef<void>& ctx,
    const std::string& host, const std::string& path);


#ifdef HAVE_OPENSSL

template <class Handshake>
ut::Task<void> asyncHandshake(asio::ssl::stream<asio::ip::tcp::socket>& socket,
    const ut::ContextRef<void>& ctx,
    Handshake type)
{
    ut::Task<void> task;
    socket.async_handshake(type, ut::makeAsioHandler(task, ctx));
    return task;
}

ut::Task<asio::ip::tcp::endpoint> asyncHttpsClientConnect(
    asio::ssl::stream<asio::ip::tcp::socket>& socket,
    const ut::ContextRef<void>& ctx,
    const std::string& host);

ut::Task<size_t> asyncHttpsDownload(asio::io_service& io, asio::streambuf& outBuf,
    const ut::ContextRef<void>& ctx,
    asio::ssl::context_base::method sslVersion,
    const std::string& host, const std::string& path);

ut::Task<void> asyncShutdown(asio::ssl::stream<asio::ip::tcp::socket>& socket,
    const ut::ContextRef<void>& ctx);

#endif // HAVE_OPENSSL

}

#endif // HAVE_BOOST
