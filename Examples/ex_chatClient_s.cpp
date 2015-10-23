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

#ifdef HAVE_BOOST_CONTEXT

#include "Common.h"
#include "util/IO.h"
#include <CppAsync/AsioWrappers.h>
#include <CppAsync/StackfulAsync.h>
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
        return mTask;
    }

    void start()
    {
        mTask = ut::stackful::startAsync([this]() {
            // Suspend until connected to server.
            ut::stackful::await_(
                ut::asyncResolveAndConnect(mCtx->socket, mCtx, mQuery));

            // Suspend until we've introduced self.
            mCtx->msg = mNickname + "\n";
            ut::stackful::await_(
                ut::asyncWrite(mCtx->socket, asio::buffer(mCtx->msg), mCtx));

            // Start input loop.
            std::thread([this] { inputFunc(); }).detach();

            // Start reader & writer coroutines.
            auto readerTask = asyncReader();
            auto writerTask = asyncWriter();

            // Suspend until /leave or exception.
            ut::stackful::awaitAny_(readerTask, writerTask);
        });
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

    ut::Task<void> asyncReader()
    {
        return ut::stackful::startAsync([this]() {
            do {
                // Suspend until a message has been read.
                ut::stackful::await_(
                    ut::asyncReadUntil(mCtx->socket, mCtx->buf, mCtx, std::string("\n")));

                std::string line;
                std::getline(std::istream(&mCtx->buf), line);

                printf("-- %s\n", line.c_str());
            } while (true);
        });
    }

    ut::Task<void> asyncWriter()
    {
        return ut::stackful::startAsync([this]() {
            bool quit = false;
            do {
                if (mMsgQueue.empty()) {
                    ut::Task<void> task;
                    mEvtMsgQueued = task.takePromise().share();

                    // Suspend while the outbound queue is empty.
                    ut::stackful::await_(task);
                } else {
                    mCtx->msg = std::move(mMsgQueue.front());
                    mMsgQueue.pop_front();

                    // Suspend until message has been sent.
                    ut::stackful::await_(
                        ut::asyncWrite(mCtx->socket, asio::buffer(mCtx->msg), mCtx));

                    if (mCtx->msg == "/leave\n")
                        quit = true;
                }
            } while (!quit);
        });
    }

    tcp::resolver::query mQuery;
    std::string mNickname;
    ut::ContextRef<Context> mCtx;
    ut::Task<void> mTask;
    std::deque<Msg> mMsgQueue;
    ut::SharedPromise<void> mEvtMsgQueued;
};

}

void ex_chatClient_s()
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

#endif // HAVE_BOOST_CONTEXT
