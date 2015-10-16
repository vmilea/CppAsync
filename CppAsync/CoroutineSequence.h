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

#include "impl/Common.h"
#include "impl/Assert.h"
#include "Coroutine.h"
#include <iterator>

namespace ut {

template <class T>
class CoroutineSequence;

template <class T>
CoroutineSequence<T> asSequence(Coroutine& coroutine) _ut_noexcept
{
    return CoroutineSequence<T>(coroutine);
}

/**
 * Wraps a generator coroutine for easier iteration
 */
template <class T>
class CoroutineSequence
{
public:
    class Iterator;
    using iterator_type = Iterator;

    CoroutineSequence(Coroutine& coroutine) _ut_noexcept
        : mCoroutine(coroutine)
        , mDidBegin(false) { }

    /**
     * Returns a forward iterator
     *
     * May only be called once. Traversing sequence multiple times is not supported.
     */
    Iterator begin()
    {
        ut_dcheck(!mDidBegin &&
            "Coroutine iteration may not be restarted");

        mDidBegin = true;

        Iterator it(&mCoroutine);
        return ++it;
    }

    /** Returns sequence end */
    Iterator end() _ut_noexcept
    {
        return Iterator(nullptr);
    }

    /**
     * Forward iterator
     */
    class Iterator
    {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        Iterator() _ut_noexcept
            : mCoroutine(nullptr) { }

        T& operator*() _ut_noexcept
        {
            ut_dcheck(mCoroutine != nullptr &&
                "May not dereference end");

            return mCoroutine->valueAs<T>();
        }

        Iterator& operator++()
        {
            ut_dcheck(mCoroutine != nullptr &&
                "May not increment past end");

            _ut_try {
                if ((*mCoroutine)()) {
                    ut_dcheck(mCoroutine->value() != nullptr &&
                        "May not yield nullptr from coroutine");
                } else {
                    mCoroutine = nullptr;
                }
            } _ut_catch (...) {
                mCoroutine = nullptr;
                _ut_rethrow;
            }

            return *this;
        }

        bool operator==(const Iterator& other) _ut_noexcept
        {
            return mCoroutine == other.mCoroutine;
        }

        bool operator!=(const Iterator& other) _ut_noexcept
        {
            return !(*this == other);
        }

    private:
        Iterator(Coroutine *coroutine) _ut_noexcept
            : mCoroutine(coroutine) { }

        // Disable postfix increment.
        Iterator& operator++(int) = delete;

        Coroutine *mCoroutine;

        friend class CoroutineSequence<T>;
    };

private:
    // Not assignable
    void operator=(const CoroutineSequence<T>& other) = delete;

    Coroutine& mCoroutine;
    bool mDidBegin;

    friend CoroutineSequence<T> asSequence<T>(Coroutine& coroutine) _ut_noexcept;
};

}
