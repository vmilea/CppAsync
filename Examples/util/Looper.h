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

#include "../Common.h"
#include <CppAsync/util/TypeTraits.h>
#include "Chrono.h"
#include "Function.h"
#include "Thread.h"
#include <cassert>
#include <utility>
#include <vector>

namespace util {

using Ticket = uint32_t;

namespace detail
{
    class LoopContext
    {
    public:
        LoopContext() _ut_noexcept
            : mTicketCounter(0) { }

        void runQueued(bool *quit)
        {
            Timepoint now = monotonicTime();

            for (auto& action : mQueuedActions) {
                if (action.isCanceled)
                    continue;

                if (action.triggerTime <= now) {
#ifdef UT_DISABLE_EXCEPTIONS
                    action.f();
#else
                    try {
                        action.f();
                    } catch (const std::exception& e) {
                        fprintf(stderr, "Uncaught exception while running loop action: %s\n",
                            e.what());
                        assert (false);
                    }
#endif
                    action.isCanceled = true;

                    if (*quit) // running the action may have triggered quit
                        break;
                }
            }
        }

        Timepoint queuePending() _ut_noexcept // must have lock
        {
            for (auto& action : mQueuedActions) {
                if (!action.isCanceled)
                    mPendingActions.push_back(std::move(action));
            }

            mQueuedActions.clear();
            mQueuedActions.swap(mPendingActions);

            auto wakeTime = Timepoint::max();

            for (auto& action : mQueuedActions) {
                if (action.triggerTime < wakeTime)
                    wakeTime = action.triggerTime;
            }

            return wakeTime;
        }

        bool hasPending() const _ut_noexcept // must have lock
        {
            return !mPendingActions.empty();
        }

        template <typename F>
        Ticket schedule(F&& f, Timepoint triggerTime) // must have lock
        {
            return scheduleImpl(std::forward<F>(f), triggerTime);
        }

        bool tryCancelQueued(Ticket ticket) _ut_noexcept
        {
            for (auto& action : mQueuedActions) {
                if (action.ticket == ticket) {
                    if (!action.isCanceled) {
                        action.isCanceled = true;
                        return true;
                    }

                    break;
                }
            }

            return false;
        }

        bool tryCancelPending(Ticket ticket) _ut_noexcept // must have lock
        {
            for (auto it = mPendingActions.begin(), end = mPendingActions.end(); it != end; ++it) {
                auto& action = *it;

                if (action.ticket == ticket) {
                    assert (!action.isCanceled);

                    mPendingActions.erase(it);
                    return true;
                }
            }

            return false;
        }

        void cancelAllQueued() _ut_noexcept
        {
            for (auto& action : mQueuedActions) {
                action.isCanceled = true;
            }
        }

        void cancelAllPending() _ut_noexcept // must have lock
        {
            mPendingActions.clear();
        }

    private:
        class ManagedAction
        {
        public:
            Ticket ticket;
            Timepoint triggerTime;
            bool isCanceled;
            Function<void ()> f;

            template <class F>
            ManagedAction(Ticket ticket, Timepoint triggerTime, const F& f)
                : ticket(ticket)
                , triggerTime(triggerTime)
                , isCanceled(false)
                , f(f) { }

            template <class F>
            ManagedAction(Ticket ticket, Timepoint triggerTime, F&& f)
                : ticket(ticket)
                , triggerTime(triggerTime)
                , isCanceled(false)
                , f(std::move(f)) { }

            ManagedAction(ManagedAction&& other) _ut_noexcept // noexcept abuse
                : ticket(other.ticket)
                , triggerTime(other.triggerTime)
                , isCanceled(other.isCanceled)
                , f(std::move(other.f)) { }

            ManagedAction& operator=(ManagedAction&& other) _ut_noexcept // noexcept abuse
            {
                ticket = other.ticket;
                triggerTime = other.triggerTime;
                isCanceled = other.isCanceled;
                f = std::move(other.f);

                return *this;
            }
        };

        template <class F>
        Ticket scheduleImpl(F&& f, Timepoint triggerTime)
        {
            mPendingActions.emplace_back(
                ++mTicketCounter, triggerTime, std::forward<F>(f));

            return mPendingActions.back().ticket;
        }

        unsigned int mTicketCounter;

        // Queued actions are accessed only from run loop thread. No locking required.
        std::vector<ManagedAction> mQueuedActions;

        // Pending actions may be enqueued from any thread. Locking is required.
        std::vector<ManagedAction> mPendingActions;
    };
}

//
// Looper
//

class Looper
{
public:
    Looper() _ut_noexcept
        : mQuit(false) { }

    ~Looper() _ut_noexcept
    {
        mContext.cancelAllQueued();

        {
            LockGuard _(mMutex);
            mContext.cancelAllPending();
        }
    }

    void run()
    {
        mThreadId = util::threading::this_thread::get_id();

        mQuit = false;
        do {
            {
                UniqueLock lock(mMutex);
                do {
                    Timepoint sleepUntil = mContext.queuePending();
                    if (sleepUntil == Timepoint::max()) {
                        mQuit = true;
                        break;
                    }

                    Timepoint now = monotonicTime();
                    if (sleepUntil <= now)
                        break;

                    Timepoint::duration timeout = sleepUntil - now;

                    if (timeout < util::chrono::milliseconds(2)) {
                        // Busy wait if less than 2ms until trigger
                        do {
                            mMutex.unlock();
                            util::threading::this_thread::yield();
                            mMutex.lock();
                        } while (monotonicTime() < sleepUntil && !mContext.hasPending());
                    } else {
                        mMutexCond.wait_for(lock, timeout);
                    }
                } while (true);
            }

            if (!mQuit)
                mContext.runQueued(&mQuit);

            // util::threading::this_thread::yield();
        } while (!mQuit);

        {
            LockGuard _(mMutex);
            mContext.queuePending(); // Delete canceled actions.
        }
    }

    void quit()
    {
        assert (util::threading::this_thread::get_id() == mThreadId &&
            "quit() called from outside the loop!");

        cancelAll();
        mQuit = true;
    }

    bool cancel(Ticket ticket)
    {
        assert (util::threading::this_thread::get_id() == mThreadId &&
            "tryCancel() called from outside the loop!");

        bool didCancel = mContext.tryCancelQueued(ticket);

        if (!didCancel) {
            {
                LockGuard _(mMutex);
                didCancel = mContext.tryCancelPending(ticket);
            }
        }

        return didCancel;
    }

    void cancelAll()
    {
        assert (util::threading::this_thread::get_id() == mThreadId &&
            "cancelAll() called from outside the loop!");

        mContext.cancelAllQueued();

        {
            LockGuard _(mMutex);
            mContext.cancelAllPending();
        }
    }

    /** thread safe */
    template <typename F>
    Ticket schedule(F&& f, long delay = 0)
    {
        // TODO: consider overflow
        Timepoint triggerTime = monotonicTime() + util::chrono::milliseconds(delay);

        {
            LockGuard _(mMutex);
            mMutexCond.notify_one();

            return mContext.schedule(std::forward<F>(f), triggerTime);
        }
    }

    /** thread safe */
    template <class F>
    Ticket post(F&& f)
    {
        return schedule(std::forward<F>(f), 0);
    }

private:
    Looper(const Looper& other) = delete;
    Looper& operator=(const Looper& other) = delete;

    using Mutex = util::threading::timed_mutex;
    using LockGuard = util::threading::lock_guard<Mutex>;
    using UniqueLock = util::threading::unique_lock<Mutex>;

    detail::LoopContext mContext;
    bool mQuit;
    util::threading::thread::id mThreadId;
    Mutex mMutex;
    util::threading::condition_variable_any mMutexCond;
};

}
