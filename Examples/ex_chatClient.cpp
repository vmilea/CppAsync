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
#include <CppAsync/AsioWrappers.h>
#include <CppAsync/StacklessAsync.h>
#include <CppAsync/util/StringUtil.h>
#include <cstdio>
#include <chrono>
#include <deque>
#include <sstream>
#include <thread>

namespace {

namespace asio = boost::asio;
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
        // Keep it simple by defining coroutine as a member function instead
        // of a full blown AsyncFrame. ChatClient holds all persistent data.
        mMainTask = ut::startAsync(this, &ChatClient::asyncMain);
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

    void asyncMain(ut::AsyncCoroState<void>& coroState)
    {
        ut::AwaitableBase *doneAwt = nullptr;
        ut_begin_function(coroState);

        mConnectTask = ut::asyncResolveAndConnect(mCtx->socket, mCtx, mQuery);
        // Suspend until connected to server.
        ut_await_(mConnectTask);

        // Suspend until we've introduced self.
        mCtx->msg = mNickname + "\n";
        mWriteTask = ut::asyncWrite(mCtx->socket, asio::buffer(mCtx->msg), mCtx);
        ut_await_(mWriteTask);

        // Start input loop.
        std::thread([this] { inputFunc(); }).detach();

        // Start reader & writer coroutines.
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
            mReadTask = ut::asyncReadUntil(mCtx->socket, mCtx->buf, mCtx, std::string("\n"));
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
                mEvtMsgQueued = mEvtTask.takePromise().share();

                // Suspend while the outbound queue is empty.
                ut_await_(mEvtTask);
            } else {
                mCtx->msg = std::move(mMsgQueue.front());
                mMsgQueue.pop_front();

                // Suspend until message has been sent.
                mWriteTask = ut::asyncWrite(mCtx->socket, asio::buffer(mCtx->msg), mCtx);
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

    tcp::resolver::query mQuery;
    std::string mNickname;
    std::deque<Msg> mMsgQueue;
    ut::SharedPromise<void> mEvtMsgQueued;
    ut::ContextRef<Context> mCtx;

    ut::Task<void> mMainTask;
    ut::Task<void> mReaderTask;
    ut::Task<void> mWriterTask;
    ut::Task<void> mEvtTask;
    ut::Task<tcp::endpoint> mConnectTask;
    ut::Task<size_t> mReadTask;
    ut::Task<size_t> mWriteTask;
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
