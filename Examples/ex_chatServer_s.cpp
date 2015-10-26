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

#ifdef HAVE_BOOST_CONTEXT

#include "Common.h"
#include "ex_chatServer.h"
#include <CppAsync/AsioWrappers.h>
#include <CppAsync/Combinators.h>
#include <CppAsync/StackfulAsync.h>
#include <CppAsync/util/ScopeGuard.h>
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
        return mMainTask;
    }

    void start()
    {
        // Start the main coroutine. Library generates a proxy Frame that
        // will invoke the given method of target object.
        mMainTask = ut::stackful::startAsync(this, &ClientSession::asyncMain);
    }

private:
    struct Context
    {
        tcp::socket socket;
        Msg msg;
        asio::streambuf buf;

        Context() : socket(sIo) { }
    };

    // Coroutine body may be defined in a separate function. Here a member
    // function of ClientSession is used for easier access.
    void asyncMain()
    {
        // Make sure socket gets closed.
        ut_scope_guard_([this] { close(); });

        // Session begins with client introducing himself.
        ut::stackful::await_(
            ut::asyncReadUntil(mCtx->socket, mCtx->buf, mCtx, std::string("\n")));
        std::getline(std::istream(&mCtx->buf), mNickname);

        // Join room and notify everybody.
        mRoom.add(this);
        ut_scope_guard_([this] { mRoom.remove(this); });

        // Start reader & writer coroutines.
        auto readerTask = ut::stackful::startAsync(this, &ClientSession::asyncReader);
        auto writerTask = ut::stackful::startAsync(this, &ClientSession::asyncWriter);

        // Suspend until /leave or exception.
        ut::stackful::awaitAny_(readerTask, writerTask);
    }

    void asyncReader()
    {
        bool quit = false;
        do {
            // Suspend until a message has been read.
            ut::stackful::await_(
                ut::asyncReadUntil(mCtx->socket, mCtx->buf, mCtx, std::string("\n")));

            std::string line;
            std::getline(std::istream(&mCtx->buf), line);

            if (line == "/leave")
                quit = true;
            else
                mRoom.broadcast(mNickname, line);
        } while (!quit);
    }

    void asyncWriter()
    {
        do {
            if (mMsgQueue.empty()) {
                ut::Task<void> evtTask;
                mEvtMsgQueued = evtTask.takePromise().share();

                // Suspend while the outbound queue is empty.
                ut::stackful::await_(evtTask);
            } else {
                mCtx->msg = std::move(mMsgQueue.front());
                mMsgQueue.pop_front();

                // Suspend until the message has been sent.
                ut::stackful::await_(
                    ut::asyncWrite(mCtx->socket, asio::buffer(mCtx->msg), mCtx));
            }
        } while (true);
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

    ChatRoom& mRoom;
    std::string mNickname;
    std::deque<Msg> mMsgQueue;
    ut::SharedPromise<void> mEvtMsgQueued;
    ut::ContextRef<Context> mCtx;

    ut::Task<void> mMainTask;
};

// Specialization allows awaiting the termination of a ClientSession.
//
inline ut::AwaitableBase& selectAwaitable(ClientSession& item)
{
    return item.task();
}

static ut::Task<void> asyncChatServer(uint16_t port)
{
    return ut::stackful::startAsync([port]() {
        struct Context
        {
            tcp::acceptor acceptor;
            std::list<std::unique_ptr<ClientSession>> sessions;

            Context() : acceptor(sIo, tcp::v4()) { }
        };
        auto ctx = ut::makeContext<Context>();

        ChatRoom room;
        std::unique_ptr<ClientSession> session;
        ut::Task<void> acceptTask;

        ctx->acceptor.bind(tcp::endpoint(tcp::v4(), port));
        ctx->acceptor.listen();

        do {
            printf("waiting for clients to connect / disconnect...\n");

            if (session == nullptr) {
                // Prepare for new connection.
                session.reset(new ClientSession(room));
                acceptTask = ut::asyncAccept(ctx->acceptor, session->socket(), ctx);
            }

            auto sessionEndedTask = ut::whenAny(ctx->sessions);

            // Suspend until a connection has been accepted or terminated.
            auto *doneTask = ut::stackful::awaitAny_(acceptTask, sessionEndedTask);

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
    });
}

}

void ex_chatServer_s()
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

#endif // HAVE_BOOST_CONTEXT
