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

#if defined(_MSC_VER) && _MSC_VER >= 1900

#include "../impl/Common.h"
#include "../util/ScopeGuard.h"
#include "../Coroutine.h"
#include <experimental/resumable>

namespace ut {

namespace detail
{
    namespace experimental
    {
        class CoroutinePromise
        {
        public:
            CoroutinePromise() _ut_noexcept
                : mCurrentValue(nullptr) { }

            Coroutine get_return_object() _ut_noexcept
            {
                class Adapter
                {
                public:
                    using handle_type = std::experimental::coroutine_handle<CoroutinePromise>;

                    Adapter(CoroutinePromise *promise) _ut_noexcept
                        : mCoro(handle_type::from_promise(promise)) { }

                    Adapter(Adapter&& other) _ut_noexcept
                        : mCoro(other.mCoro)
                    {
                        other.mCoro = nullptr;
                    }

                    Adapter& operator=(Adapter&& other) _ut_noexcept
                    {
                        ut_assert(this != &other);

                        if (mCoro)
                            mCoro.destroy();

                        mCoro = other.mCoro;
                        other.mCoro = nullptr;
                    }

                    ~Adapter() _ut_noexcept
                    {
                        if (mCoro)
                            mCoro.destroy();
                    }

                    bool isDone() const _ut_noexcept
                    {
                        ut_assert(mCoro);

                        return mCoro.done();
                    }

                    void* value() const _ut_noexcept
                    {
                        return mCoro.promise().mCurrentValue;
                    }

                    bool operator()(void *arg)
                    {
                        ut_assert(arg == nullptr &&
                            "Can't yield values to experimental coroutine");
                        ut_assert(mCoro);

                        ut_scope_guard_([this] {
                            if (mCoro.done()) {
                                mCoro.destroy();
                                mCoro = nullptr;
                            }
                        });

                        mCoro.resume();

                        return !isDone();
                    }

                private:
                    handle_type mCoro;
                };

                return Coroutine::wrap(Adapter(this));
            }

            bool initial_suspend() const _ut_noexcept
            {
                return true;
            }

            bool final_suspend() const _ut_noexcept
            {
                return true;
            }

            void yield_value(void *value)
            {
                mCurrentValue = value;
            }

        private:
            CoroutinePromise(const CoroutinePromise& other) = delete;
            CoroutinePromise& operator=(const CoroutinePromise& other) = delete;

            void *mCurrentValue;
        };
    }
}

}

namespace std {

namespace experimental
{
    template <class ...Args>
    struct coroutine_traits<ut::Coroutine, Args...>
    {
        using promise_type = ut::detail::experimental::CoroutinePromise;

        template <class Alloc, class ...Args>
        static Alloc get_allocator(std::allocator_arg_t, const Alloc& alloc, Args&&...)
        {
            return allocator;
        }
    };
}

}

#endif
