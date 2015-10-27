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
#include <CppAsync/asio/Asio.h>
#include <string>

#ifdef HAVE_OPENSSL
#include <boost/asio/ssl.hpp>
#endif

namespace util { namespace asio {

namespace detail
{
    template <class Socket>
    ut::Task<std::size_t> asyncHttpGetImpl(
        Socket& socket, boost::asio::streambuf& outBuf,
        const std::string& host, const std::string& path, bool persistentConnection, bool readAll,
        const ut::ContextRef<void>& ctx);
}

template <class Socket>
ut::Task<std::size_t> asyncHttpGetHeader(
    Socket& socket, boost::asio::streambuf& outBuf,
    const std::string& host, const std::string& path, bool persistentConnection,
    const ut::ContextRef<void>& ctx)
{
    return detail::asyncHttpGetImpl(socket, outBuf,
        host, path, persistentConnection, false, ctx);
}

template <class Socket>
ut::Task<std::size_t> asyncHttpGet(
    Socket& socket, boost::asio::streambuf& outBuf,
    const std::string& host, const std::string& path, bool persistentConnection,
    const ut::ContextRef<void>& ctx)
{
    return detail::asyncHttpGetImpl(socket, outBuf,
        host, path, persistentConnection, true, ctx);
}

ut::Task<std::size_t> asyncHttpDownload(
    boost::asio::io_service& io, boost::asio::streambuf& outBuf,
    const std::string& host, const std::string& path,
    const ut::ContextRef<void>& ctx);


#ifdef HAVE_OPENSSL

ut::Task<boost::asio::ip::tcp::endpoint> asyncHttpsClientConnect(
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket>& socket,
    const std::string& host,
    const ut::ContextRef<void>& ctx);

ut::Task<std::size_t> asyncHttpsDownload(
    boost::asio::io_service& io, boost::asio::streambuf& outBuf,
    boost::asio::ssl::context_base::method sslVersion,
    const std::string& host, const std::string& path,
    const ut::ContextRef<void>& ctx);

#endif // HAVE_OPENSSL

} } // util::asio

#endif // HAVE_BOOST
