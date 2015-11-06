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

/** See: http://www.boost.org/doc/libs/develop/doc/html/boost_asio/example/cpp11/chat/ */

#ifdef HAVE_BOOST

#include "Common.h"
#include "util/IO.h"
#include <CppAsync/StacklessAsync.h>
#include <CppAsync/Boost/Asio.h>
#include <CppAsync/util/StringUtil.h>
#include <cstdio>
#include <chrono>
#include <deque>
#include <sstream>
#include <thread>

namespace {

namespace asio {
    using namespace boost::asio;
    using namespace ut::asio;
}
using asio::ip::tcp;

static asio::io_service sIo;

// Single line message
//
using Msg = std::string;

// Chat client wrapper
//
class ChatClient
{
public:
    ChatClient(std::string host, uint16_t port, std::string nickname)
        : mQuery(tcp::v4(), host, ut::string_printf("%u", port))
        , mNickname(nickname)
        , mCtx(ut::makeContext<Context>()) { }

    ut::AwaitableBase& task()
    {
        return mMainTask;
    }

    void start()
    {
        struct Frame : ut::AsyncFrame<void>
        {
            Frame(ChatClient *thiz) : thiz(thiz) { }

            // Make sure socket gets closed after task completes / fails.
            ~Frame() { thiz->close(); }

            // This Frame is just a proxy. ChatClient holds the actual
            // coroutine function and persisted data.
            void operator()() { thiz->asyncMain(coroState()); }

        private:
            ChatClient *thiz;
        };

        mMainTask = ut::startAsyncOf<Frame>(this);
    }

private:
    struct Context
    {
        tcp::socket socket;
        tcp::resolver resolver;
        asio::streambuf buf;
        Msg msg;

        Context() : socket(sIo), resolver(sIo) { }
    };

    // Coroutine body may be defined in a separate function. Here a member
    // function of ChatClient is used for easier access.
    void asyncMain(ut::AsyncCoroState<void>& coroState)
    {
        ut::AwaitableBase *doneAwt = nullptr;
        ut_begin_function(coroState);

        mConnectTask = asio::asyncResolveAndConnect(mCtx->socket, mQuery, mCtx);
        // Suspend until connected to server.
        ut_await_(mConnectTask);

        // Suspend until we've introduced self.
        mCtx->msg = mNickname + "\n";
        mWriteTask = asio::async_write(mCtx->socket, asio::buffer(mCtx->msg), asio::asTask[mCtx]);
        ut_await_(mWriteTask);

        // Start input loop.
        std::thread([this] { inputFunc(); }).detach();

        // Start reader & writer coroutines. Library generates a proxy Frame
        // that will invoke the given method of target object.
        mReaderTask = ut::startAsync(this, &ChatClient::asyncReader);
        mWriterTask = ut::startAsync(this, &ChatClient::asyncWriter);

        // Suspend until /leave or exception.
        ut_await_any_(doneAwt, mReaderTask, mWriterTask);

        ut_end();
    }

    void asyncReader(ut::AsyncCoroState<void>& coroState)
    {
        std::string line;
        ut_begin_function(coroState);

        do {
            // Suspend until a message has been read.
            mReadTask = asio::async_read_until(mCtx->socket, mCtx->buf, std::string("\n"),
                asio::asTask[mCtx]);
            ut_await_(mReadTask);

            std::getline(std::istream(&mCtx->buf), line);

            printf("-- %s\n", line.c_str());
        } while (true);

        ut_end();
    }

    void asyncWriter(ut::AsyncCoroState<void>& coroState)
    {
        ut_begin_function(coroState);

        do {
            if (mMsgQueue.empty()) {
                mEvtTask = ut::Task<void>();
                mEvtMsgQueued = mEvtTask.takePromise();

                // Suspend while the outbound queue is empty.
                ut_await_(mEvtTask);
            } else {
                mCtx->msg = std::move(mMsgQueue.front());
                mMsgQueue.pop_front();

                // Suspend until message has been sent.
                mWriteTask = asio::async_write(mCtx->socket, asio::buffer(mCtx->msg),
                    asio::asTask[mCtx]);
                ut_await_(mWriteTask);

                if (mCtx->msg == "/leave\n")
                    break;
            }
        } while (true);

        ut_end();
    }

    void inputFunc()
    {
        // Sleep to tidy up output.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        printf(" > ");

        do {
            std::string line = util::readLine();

            // Process the message on main loop.
            sIo.post([this, line]() {
                mMsgQueue.push_back(line + "\n");

                // Notify writer.
                mEvtMsgQueued();
            });

            // Sleep to tidy up output.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            printf (" > ");
        } while(true);
    }

    void close()
    {
        try {
            mCtx->socket.shutdown(tcp::socket::shutdown_both);
            mCtx->socket.close();
        } catch (...) {
            fprintf(stderr, "failed to close socket!\n");
        }
    }

    tcp::resolver::query mQuery;
    std::string mNickname;
    std::deque<Msg> mMsgQueue;
    ut::Promise<void> mEvtMsgQueued;
    ut::ContextRef<Context> mCtx;

    ut::Task<void> mMainTask;
    ut::Task<void> mReaderTask;
    ut::Task<void> mWriterTask;
    ut::Task<void> mEvtTask;
    ut::Task<tcp::endpoint> mConnectTask;
    ut::Task<std::size_t> mReadTask;
    ut::Task<std::size_t> mWriteTask;
};

}

void ex_chatClient()
{
    printf("enter your nickname: ");
    std::string nickname = util::readLine();

    ChatClient client("localhost", 3455, nickname);
    client.start();

    // Loop until there are no more scheduled operations.
    sIo.run();

    auto& task = client.task();
    assert(task.isReady());

    if (task.hasError()) {
        try {
            std::rethrow_exception(task.error());
        } catch (std::exception& e) {
            printf("chat client failed: %s\n", e.what());
        } catch (...) {
            printf("chat client failed: unknown exception\n");
        }
    }
}

#endif // HAVE_BOOST
