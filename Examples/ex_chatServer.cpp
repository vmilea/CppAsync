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
#include "ex_chatServer.h"
#include <CppAsync/AsioWrappers.h>
#include <CppAsync/Combinators.h>
#include <CppAsync/StacklessAsync.h>
#include <cstdio>
#include <deque>
#include <list>

namespace {

namespace asio = boost::asio;
using asio::ip::tcp;

static asio::io_service sIo;

// ClientSession handles communication with an single guest.
//
class ClientSession : public Guest
{
public:
    ClientSession(ChatRoom& room)
        : mRoom(room)
        , mCtx(ut::makeContext<Context>()) { }

    // Deleting the session will cancel its Task and force unwinding of coroutines.
    ~ClientSession() { }

    const char* nickname() final
    {
        return mNickname.c_str();
    }

    void push(const Msg& msg) final
    {
        mMsgQueue.push_back(msg);

        // Notify writer.
        mEvtMsgQueued();
    }

    tcp::socket& socket()
    {
        return mCtx->socket;
    }

    ut::AwaitableBase& task()
    {
        return mTask;
    }

    void start()
    {
        mTask = ut::startAsync<MainFrame>(this);
    }

private:
    struct Context
    {
        tcp::socket socket;
        Msg msg;
        asio::streambuf buf;

        Context() : socket(sIo) { }
    };

    struct MainFrame : ut::AsyncFrame<void>
    {
        MainFrame(ClientSession *thiz) : thiz(thiz) { }

        void operator()()
        {
            ut::AwaitableBase *doneAwt = nullptr;
            auto& ctx = thiz->mCtx;
            ut_begin();

            // Session begins with client introducing himself.
            transferTask = ut::asyncReadUntil(ctx->socket, ctx->buf, ctx, std::string("\n"));
            ut_await_(transferTask);
            std::getline(std::istream(&ctx->buf), thiz->mNickname);

            // Join room and notify everybody.
            thiz->mRoom.add(thiz);

            // Start reader & writer coroutines.
            readerTask = ut::startAsync<ReaderFrame>(thiz);
            writerTask = ut::startAsync<WriterFrame>(thiz);

            // Suspend until /leave or exception.
            ut_await_any_(doneAwt, readerTask, writerTask);

            ut_try {
                // Suspend until /leave or exception.
                ut_await_any_(doneAwt, readerTask, writerTask);

                thiz->mRoom.remove(thiz);
            } ut_catch (...) {
                thiz->mRoom.remove(thiz);
                throw;
            }

            ut_end();
        }

    private:
        ClientSession *thiz;
        ut::Task<size_t> transferTask;
        ut::Task<void> readerTask;
        ut::Task<void> writerTask;
    };

    struct ReaderFrame : ut::AsyncFrame<void>
    {
        ReaderFrame(ClientSession *thiz) : thiz(thiz) { }

        void operator()()
        {
            std::string line;
            auto& ctx = thiz->mCtx;
            ut_begin();

            do {
                // Suspend until a message has been read.
                transferTask = ut::asyncReadUntil(ctx->socket, ctx->buf, ctx, std::string("\n"));
                ut_await_(transferTask);

                std::getline(std::istream(&ctx->buf), line);

                if (line == "/leave")
                    break;
                else
                    thiz->mRoom.broadcast(thiz->mNickname, line);
            } while (true);

            ut_end();
        }

    private:
        ClientSession *thiz;
        ut::Task<size_t> transferTask;
    };

    struct WriterFrame : ut::AsyncFrame<void>
    {
        WriterFrame(ClientSession *thiz)
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
                }
            } while (true);

            ut_end();
        }

    private:
        ClientSession *thiz;
        ut::Task<void> evtTask;
        ut::Task<size_t> transferTask;
    };

    ChatRoom& mRoom;
    ut::ContextRef<Context> mCtx;
    std::string mNickname;
    ut::Task<void> mTask;
    std::deque<Msg> mMsgQueue;
    ut::SharedPromise<void> mEvtMsgQueued;
};

// Specialization allows awaiting the termination of a ClientSession.
//
inline ut::AwaitableBase& selectAwaitable(ClientSession& item)
{
    return item.task();
}

static ut::Task<void> asyncChatServer(uint16_t port)
{
    struct Frame : ut::AsyncFrame<void>
    {
        Frame(uint16_t port)
            : port(port)
            , ctx(ut::makeContext<Context>()) { }

        void operator()()
        {
            ut::AwaitableBase *doneTask = nullptr;
            ut_begin();

            ctx->acceptor.bind(tcp::endpoint(tcp::v4(), port));
            ctx->acceptor.listen();

            do {
                printf("waiting for clients to connect / disconnect...\n");

                if (session == nullptr) {
                    // Prepare for new connection.
                    session.reset(new ClientSession(room));
                    acceptTask = ut::asyncAccept(ctx->acceptor, session->socket(), ctx);
                }

                sessionEndedTask = ut::whenAny(ctx->sessions);

                // Suspend until a connection has been accepted or terminated.
                ut_await_any_(doneTask, acceptTask, sessionEndedTask);

                if (doneTask == &acceptTask) {
                    sessionEndedTask.cancel();

                    if (acceptTask.hasError()) {
                        printf("failed to accept client\n");
                    } else {
                        printf("client accepted\n");

                        // Start client session.
                        session->start();
                        ctx->sessions.push_back(std::move(session));
                    }

                    session = nullptr;
                } else {
                    // Remove terminated session.
                    auto pos = sessionEndedTask.get();
                    ClientSession& endedSession = **pos;

                    if (endedSession.task().hasError())
                        printf("client '%s' has disconnected\n", endedSession.nickname());
                    else
                        printf("client '%s' has left\n", endedSession.nickname());

                    ctx->sessions.erase(pos);
                }
            } while (true);

            ut_end();
        }

    private:
        using session_list_type = std::list<std::unique_ptr<ClientSession>>;

        struct Context
        {
            tcp::acceptor acceptor;
            session_list_type sessions;

            Context() : acceptor(sIo, tcp::v4()) { }
        };

        uint16_t port;
        ut::ContextRef<Context> ctx;
        ChatRoom room;
        std::unique_ptr<ClientSession> session;
        ut::Task<void> acceptTask;
        ut::Task<typename session_list_type::iterator> sessionEndedTask;
    };

    return ut::startAsync<Frame>(port);
}

}

void ex_chatServer()
{
    ut::Task<void> task = asyncChatServer(3455);

    // Loop until there are no more scheduled operations.
    sIo.run();

    assert(task.isReady());
    try {
        task.get();
    } catch (std::exception& e) {
        printf("chat server failed: %s\n", e.what());
    } catch (...) {
        printf("chat server failed: unknown exception\n");
    }
}

#endif // HAVE_BOOST
