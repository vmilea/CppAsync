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

#ifdef HAVE_BOOST

#include "Common.h"
#include "util/AsioHttp.h"
#include <CppAsync/StacklessAsync.h>
#include <CppAsync/Boost/Asio.h>
#include <fstream>

namespace {

namespace asio {
    using namespace boost::asio;
    using namespace ut::asio;
    using namespace util::asio;
}
using asio::ip::tcp;

static asio::io_service sIo;

static ut::Task<void> asyncHttpDownload(asio::streambuf& outBuf,
    std::string host, std::string path)
{
    struct Frame : ut::AsyncFrame<void>
    {
        Frame(asio::streambuf& outBuf, std::string host, std::string path)
            : outBuf(outBuf)
            , host(std::move(host))
            , path(std::move(path))
            , ctx(ut::makeContext<Context>()) { }

        void operator()()
        {
            ut_begin();

            // Any Boost.Asio async function may take ut::asio::asTask instead of a regular
            // completion handler, in order to produce a Task that is hooked up to the async
            // operation. See AsioTraits.h for inner magic.
            //
            // Boost context (like sockets, resolvers, data buffers) must be kept alive until
            // ASIO callback even if Task gets canceled and coroutine deleted. This is ensured
            // by having asTask[ctx] capture a shared_ptr to the context within the completion
            // handler.
            //
            resolveTask = ctx->resolver.async_resolve(
                asio::ip::tcp::resolver::query(host, "http"),
                asio::asTask[ctx]);
            ut_await_(resolveTask);

            // Compound operations can be abstracted as subtasks.
            connectTask = asio::asyncConnectToAny(ctx->socket, resolveTask.get(), ctx);
            ut_await_(connectTask);

            downloadTask = asio::asyncHttpGet(ctx->socket, outBuf, host, path, false, ctx);
            ut_await_(downloadTask);

            ut_end();
        }

    private:
        struct Context
        {
            tcp::socket socket;
            tcp::resolver resolver;

            Context() : socket(sIo), resolver(sIo) { }
        };

        asio::streambuf& outBuf;
        const std::string host, path;
        ut::ContextRef<Context> ctx;

        ut::Task<tcp::resolver::iterator> resolveTask, connectTask;
        ut::Task<std::size_t> downloadTask;
    };

    return ut::startAsyncOf<Frame>(outBuf, std::move(host), std::move(path));
}

}

void ex_http()
{
    asio::streambuf buf;

    auto task = asyncHttpDownload(buf,
        "www.google.com",
        "/images/branding/googlelogo/2x/googlelogo_color_272x92dp.png");

    sIo.run();

    assert(task.isReady());

    try {
        task.get();

        printf("saving download.png (%d bytes)...\n", (int) buf.size());

        std::ofstream fout("download.png", std::ios::out | std::ios::binary);
        fout << &buf;
    } catch (std::exception& e) {
        printf("HTTP download failed: %s\n", e.what());
        return;
    }
}

#endif // HAVE_BOOST
