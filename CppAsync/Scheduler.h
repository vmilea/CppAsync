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

#pragma once

#include "impl/Common.h"
#include "impl/Assert.h"
#include "util/Meta.h"
#include <functional>
#include <memory>

namespace ut {

/** This function must be defined in user code */
template <class F>
void schedule(F&& action);

/**
 * Handle for a scheduled action. Destroying or resetting the
 * ticket will cancel the action
 */
class SchedulerTicket;


namespace detail
{
    struct ScheduledItem
    {
        SchedulerTicket *ticket;

        ScheduledItem() _ut_noexcept
            : ticket(nullptr) { }

    private:
        ScheduledItem(const ScheduledItem&) = delete;
        ScheduledItem& operator=(const ScheduledItem&) = delete;
    };
}

class SchedulerTicket
{
public:
    SchedulerTicket() _ut_noexcept { }

    SchedulerTicket(SchedulerTicket&& other) _ut_noexcept
        : mHandle(std::move(other.mHandle))
    {
        if ((bool) mHandle)
            mHandle->ticket = this;
    }

    SchedulerTicket& operator=(SchedulerTicket&& other) _ut_noexcept
    {
        ut_assert(this != &other);

        mHandle = std::move(other.mHandle);

        if ((bool) mHandle)
            mHandle->ticket = this;

        return *this;
    }

    operator bool() const _ut_noexcept
    {
        return (bool) mHandle;
    }

    void reset() _ut_noexcept
    {
        mHandle.reset();
    }

private:
    SchedulerTicket(const SchedulerTicket& other) = delete;
    SchedulerTicket& operator=(const SchedulerTicket& other) = delete;

    explicit SchedulerTicket(std::shared_ptr<detail::ScheduledItem> handle) _ut_noexcept
        : mHandle(std::move(handle))
    {
        mHandle->ticket = this;
    }

    std::shared_ptr<detail::ScheduledItem> mHandle;

    template <class F>
    friend SchedulerTicket scheduleWithTicket(F&& f);
};

namespace detail
{
    template <class F>
    struct ScheduledFunction : ScheduledItem
    {
        F f;

        explicit ScheduledFunction(F&& f)
            : f(std::move(f)) { }

        explicit ScheduledFunction(const F& f)
            : f(f) { }
    };

    template <typename F>
    class WeakAction
    {
    public:
        explicit WeakAction(std::weak_ptr<ScheduledFunction<F>> handle) _ut_noexcept
            : mHandle(std::move(handle)) { }

        WeakAction(const WeakAction& other) _ut_noexcept
            : mHandle(other.mHandle) { }

        WeakAction(WeakAction&& other) _ut_noexcept
            : mHandle(std::move(other.mHandle)) { }

        WeakAction& operator=(const WeakAction& other) _ut_noexcept
        {
            mHandle = other.mHandle;

            return *this;
        }

        WeakAction& operator=(WeakAction&& other) _ut_noexcept
        {
            ut_assert(this != &other);

            mHandle = std::move(other.mHandle);

            return *this;
        }

        void operator()() const
        {
            auto handle = mHandle.lock();
            mHandle.reset();

            if ((bool) handle) {
                ut_assert(handle->ticket != nullptr);

                // Reset ticket to prevent dangling of functor after call
                handle->ticket->reset();
                handle->f();
            }
        }

    private:
        mutable std::weak_ptr<ScheduledFunction<F>> mHandle;
    };
}


/**
* Binds scheduled action to a ticket. If the ticket is destroyed
* before the action has run, the action will be skipped.
*/
template <class F>
SchedulerTicket scheduleWithTicket(F&& action)
{
    using f_type = Unqualified<F>;
    using scheduled_type = detail::ScheduledFunction<f_type>;

    auto handle = std::make_shared<scheduled_type>(std::forward<F>(action));

    SchedulerTicket ticket(handle);

    schedule(detail::WeakAction<f_type>(
        std::weak_ptr<scheduled_type>(handle)));

    return ticket;

}

}
