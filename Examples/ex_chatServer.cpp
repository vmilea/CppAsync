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
#include "ex_chatServer.h"
#include <CppAsync/Combinators.h>
#include <CppAsync/StacklessAsync.h>
#include <CppAsync/asio/Asio.h>
#include <cstdio>
#include <deque>
#include <list>

namespace {

namespace asio {
    using namespace boost::asio;
    using namespace ut::asio;
}
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
        struct Frame : ut::AsyncFrame<void>
        {
            Frame(ClientSession *thiz) : thiz(thiz) { }

            // Make sure socket gets closed after task completes / fails.
            ~Frame() { thiz->close(); }

            // This Frame is just a proxy. ClientSession holds the actual
            // coroutine function and persisted data.
            void operator()() { thiz->asyncMain(coroState()); }

        private:
            ClientSession *thiz;
        };

        mMainTask = ut::startAsyncOf<Frame>(this);
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
    void asyncMain(ut::AsyncCoroState<void>& coroState)
    {
        ut::AwaitableBase *doneAwt = nullptr;
        ut_begin_function(coroState);

        // Session begins with client introducing himself.
        mReadTask = asio::async_read_until(mCtx->socket, mCtx->buf, std::string("\n"),
            asio::asTask[mCtx]);
        ut_await_(mReadTask);
        std::getline(std::istream(&mCtx->buf), mNickname);

        // Join room and notify everybody.
        mRoom.add(this);

        // Start reader & writer coroutines. Library generates a proxy Frame
        // that will invoke the given method of target object.
        mReaderTask = ut::startAsync(this, &ClientSession::asyncReader);
        mWriterTask = ut::startAsync(this, &ClientSession::asyncWriter);

        ut_try {
            // Suspend until /leave or exception.
            ut_await_any_(doneAwt, mReaderTask, mWriterTask);

            mRoom.remove(this);
        } ut_catch (...) {
            mRoom.remove(this);
            throw;
        }

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

            if (line == "/leave")
                break;
            else
                mRoom.broadcast(mNickname, line);
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
            }
        } while (true);

        ut_end();
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
    ut::Promise<void> mEvtMsgQueued;
    ut::ContextRef<Context> mCtx;

    ut::Task<void> mMainTask;
    ut::Task<void> mReaderTask;
    ut::Task<void> mWriterTask;
    ut::Task<void> mEvtTask;
    ut::Task<std::size_t> mReadTask;
    ut::Task<std::size_t> mWriteTask;
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
            auto& session = ctx->session;
            ut_begin();

            ctx->acceptor.bind(tcp::endpoint(tcp::v4(), port));
            ctx->acceptor.listen();

            do {
                printf("waiting for clients to connect / disconnect...\n");

                if (session == nullptr) {
                    // Prepare for new connection.
                    session.reset(new ClientSession(room));
                    acceptTask = ctx->acceptor.async_accept(session->socket(), asio::asTask[ctx]);
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
            std::unique_ptr<ClientSession> session;
            session_list_type sessions;

            Context() : acceptor(sIo, tcp::v4()) { }
        };

        uint16_t port;
        ChatRoom room;
        ut::ContextRef<Context> ctx;

        ut::Task<void> acceptTask;
        ut::Task<typename session_list_type::iterator> sessionEndedTask;
    };

    return ut::startAsyncOf<Frame>(port);
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
