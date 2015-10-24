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
        return mTask;
    }

    void start()
    {
        mTask = ut::startAsyncOf<MainFrame>(this);
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

    struct MainFrame : ut::AsyncFrame<void>
    {
        MainFrame(ChatClient *thiz) : thiz(thiz) { }

        void operator()()
        {
            ut::AwaitableBase *doneAwt = nullptr;
            auto& ctx = thiz->mCtx;
            ut_begin();

            connectTask = ut::asyncResolveAndConnect(ctx->socket, ctx, thiz->mQuery);
            // Suspend until connected to server.
            ut_await_(connectTask);

            // Suspend until we've introduced self.
            ctx->msg = thiz->mNickname + "\n";
            transferTask = ut::asyncWrite(ctx->socket, asio::buffer(ctx->msg), ctx);
            ut_await_(transferTask);

            // Start input loop.
            std::thread([this] { thiz->inputFunc(); }).detach();

            // Start reader & writer coroutines.
            readerTask = ut::startAsyncOf<ReaderFrame>(thiz);
            writerTask = ut::startAsyncOf<WriterFrame>(thiz);

            // Suspend until /leave or exception.
            ut_await_any_(doneAwt, readerTask, writerTask);

            ut_end();
        }

    private:
        ChatClient *thiz;
        ut::Task<tcp::endpoint> connectTask;
        ut::Task<size_t> transferTask;
        ut::Task<void> readerTask;
        ut::Task<void> writerTask;
    };

    struct ReaderFrame : ut::AsyncFrame<void>
    {
        ReaderFrame(ChatClient *thiz) : thiz(thiz) { }

        void operator()()
        {
            auto& ctx = thiz->mCtx;
            ut_begin();

            do {
                // Suspend until a message has been read.
                transferTask = ut::asyncReadUntil(ctx->socket, ctx->buf, ctx, std::string("\n"));
                ut_await_(transferTask);

                {
                    std::string line;
                    std::getline(std::istream(&ctx->buf), line);

                    printf("-- %s\n", line.c_str());
                }
            } while (true);

            ut_end();
        }

    private:
        ChatClient *thiz;
        ut::Task<size_t> transferTask;
    };

    struct WriterFrame : ut::AsyncFrame<void>
    {
        WriterFrame(ChatClient *thiz)
            : thiz(thiz) { }

        void operator()()
        {
            auto& ctx = thiz->mCtx;
            ut_begin();

            do {
                if (thiz->mMsgQueue.empty()) {
                    evtTask = ut::Task<void>();
                    thiz->mEvtMsgQueued = evtTask.takePromise().share();

                    // Suspend while the outbound queue is empty.
                    ut_await_(evtTask);
                } else {
                    ctx->msg = std::move(thiz->mMsgQueue.front());
                    thiz->mMsgQueue.pop_front();

                    // Suspend until message has been sent.
                    transferTask = ut::asyncWrite(ctx->socket, asio::buffer(ctx->msg), ctx);
                    ut_await_(transferTask);

                    if (ctx->msg == "/leave\n")
                        break;
                }
            } while (true);

            ut_end();
        }

    private:
        ChatClient *thiz;
        ut::Task<void> evtTask;
        ut::Task<size_t> transferTask;
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

    tcp::resolver::query mQuery;
    std::string mNickname;
    ut::ContextRef<Context> mCtx;
    ut::Task<void> mTask;
    std::deque<Msg> mMsgQueue;
    ut::SharedPromise<void> mEvtMsgQueued;
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
