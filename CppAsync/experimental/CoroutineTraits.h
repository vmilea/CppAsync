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

#if defined(_MSC_VER) && _MSC_FULL_VER >= 190024120

#include "../impl/Common.h"
#include "../Coroutine.h"
#include <experimental/coroutine>

namespace ut {

namespace detail
{
    namespace experimental
    {
        class CoroutinePromise
        {
        public:
            CoroutinePromise() noexcept
                : mCurrentValue(nullptr) { }

            Coroutine get_return_object() noexcept
            {
                class Adapter
                {
                public:
                    using handle_type = std::experimental::coroutine_handle<CoroutinePromise>;

                    Adapter(CoroutinePromise& promise) noexcept
                        : mCoro(handle_type::from_promise(promise)) { }

                    Adapter(Adapter&& other) noexcept
                        : mCoro(other.mCoro)
                    {
                        other.mCoro = nullptr;
                    }

                    Adapter& operator=(Adapter&& other) noexcept
                    {
                        ut_assert(this != &other);

                        if (mCoro)
                            mCoro.destroy();

                        mCoro = other.mCoro;
                        other.mCoro = nullptr;
                    }

                    ~Adapter() noexcept
                    {
                        if (mCoro)
                            mCoro.destroy();
                    }

                    bool isDone() const noexcept
                    {
                        ut_assert(!mCoro || !mCoro.done());

                        return !mCoro;
                    }

                    void* value() const noexcept
                    {
                        ut_dcheck(!isDone() &&
                            "value() is available only for suspended coroutines");
                        ut_assert(!mCoro.done());

                        return mCoro.promise().mCurrentValue;
                    }

                    bool operator()(void *arg)
                    {
                        ut_dcheck(arg == nullptr &&
                            "Can't yield values to experimental coroutine");
                        ut_dcheck(!isDone() &&
                            "Coroutine has finished and may not be resumed");
                        ut_assert(!mCoro.done());

                        mCoro.resume();

#ifdef UT_NO_EXCEPTIONS
                        if (mCoro.done()) {
                            mCoro.destroy();
                            mCoro = nullptr;
                        }
#else
                        if (mCoro.done()) {
                            if (mCoro.promise().mError == nullptr) {
                                mCoro.destroy();
                                mCoro = nullptr;
                            } else {
                                auto error = mCoro.promise().mError;
                                mCoro.destroy();
                                mCoro = nullptr;
                                rethrowException(std::move(error));
                            }
                        } else {
                            ut_assert(mCoro.promise().mError == nullptr);
                        }
#endif

                        return !isDone();
                    }

                private:
                    handle_type mCoro;
                };

                return Coroutine::wrap(Adapter(*this));
            }

#ifdef UT_NO_EXCEPTIONS
            static Coroutine get_return_object_on_allocation_failure() noexcept
            {
                return Coroutine(); // Return an invalid coroutine.
            }
#endif

            auto initial_suspend() const noexcept
            {
                return std::experimental::suspend_always();
            }

            auto final_suspend() const noexcept
            {
                return std::experimental::suspend_always();
            }

            auto yield_value(void *value) noexcept
            {
                mCurrentValue = value;
                return std::experimental::suspend_always();
            }

#ifndef UT_NO_EXCEPTIONS
            void set_exception(Error error) noexcept
            {
                mError = std::move(error);
            }
#endif

        private:
            CoroutinePromise(const CoroutinePromise& other) = delete;
            CoroutinePromise& operator=(const CoroutinePromise& other) = delete;

            void *mCurrentValue;

#ifndef UT_NO_EXCEPTIONS
            Error mError;
#endif
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
    };

    // Custom allocators are supported by specializing coroutine_traits according
    // to coroutine function signature. Sketch:
    //
    // template <class Alloc, class... Args>
    // struct coroutine_traits<ut::Coroutine, std::allocator_arg_t, Alloc, Args...>
    // {
    //     using alloc_type = typename std::allocator_traits<ut::Unqualified<Alloc>>
    //         ::template rebind_alloc<char>;
    //
    //     struct promise_type : ut::detail::experimental::CoroutinePromise
    //     {
    //         void* operator new(size_t size,
    //             std::allocator_arg_t, alloc_type alloc, Args&&... args)
    //         {
    //             return alloc.allocate(size);
    //         }
    //
    //         void operator delete(void *p, size_t size)
    //         {
    //             alloc_t alloc;
    //             alloc.deallocate(static_cast<char*>(p), size);
    //         }
    //     };
    // };
    //
    // Stateful allocators should be stored as part of the allocation in order to
    // be accessible from delete operator.
}

}

#endif
