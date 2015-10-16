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

/**
 * @file  ScopeGuard.h
 *
 * Declares the ScopeGuard class
 *
 */

#pragma once

#include "../impl/Common.h"
#include "../impl/Assert.h"
#include "../Log.h"

namespace ut {

/** Classic scope guard for RAII */
template <class F>
class ScopeGuard
{
public:
    using type = ScopeGuard<F>;

    /**
     * Create a dummy scope guard
     */
    explicit ScopeGuard()
        : mIsDismissed(true) { }

    /**
     * Create a scope guard
     * @param cleanup   functor to call at end of scope
     */
    explicit ScopeGuard(const F& cleanup)
        : mIsDismissed(false)
        , mCleanup(cleanup) { }

    /**
     * Create a scope guard
     * @param cleanup   functor to call at end of scope
     */
    explicit ScopeGuard(F&& cleanup)
        : mIsDismissed(false)
        , mCleanup(std::move(cleanup)) { }

    /**
     * Move constructor
     */
    ScopeGuard(ScopeGuard&& other)
        : mIsDismissed(other.mIsDismissed)
        , mCleanup(std::move(other.mCleanup))
    {
        other.mIsDismissed = true;
    }

    /**
     * Move assignment
     */
    ScopeGuard& operator=(ScopeGuard&& other)
    {
        mIsDismissed = other.mIsDismissed;
        other.mIsDismissed = true;
        mCleanup = std::move(other.mCleanup);

        return *this;
    }

    /** Perform cleanup unless dismissed */
    ~ScopeGuard()
    {
        if (!mIsDismissed) {
#ifdef UT_DISABLE_EXCEPTIONS
            mCleanup();
#else
            try {
                mCleanup();
            } catch (const std::exception& ex) {
                (void) ex;
                ut_dcheckf(false,
                    "ScopeGuard caught an exception: '%s'", ex.what());

            } catch (...) {
                ut_dcheck(false &&
                    "ScopeGuard caught exception");
            }
#endif
        }
    }

    /* Dismiss guard */
    void dismiss() const _ut_noexcept
    {
        mIsDismissed = true;
    }

    /* Check if dismissed */
    bool isDismissed() const _ut_noexcept
    {
        return mIsDismissed;
    }

    void touch() const _ut_noexcept { /* avoids "variable unused" compiler warnings */ }

private:
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    mutable bool mIsDismissed;
    F mCleanup;
};

/** Create a scope guard with template argument deduction */
template <class F>
ScopeGuard<F> makeScopeGuard(F cleanup)
{
    return ScopeGuard<F>(cleanup);
}

}

//
// These macros should be enough unless you intend to move ScopeGuard
//

/** Macro for creating anonymous scope guards */
#define ut_scope_guard_(cleanup) \
    const auto& _ut_anonymous_label(scopeGuard) = ut::makeScopeGuard(cleanup); \
    _ut_anonymous_label(scopeGuard).touch()

/** Macro for creating named scope guards */
#define ut_named_scope_guard_(name, cleanup) \
    const auto& name = ut::makeScopeGuard(cleanup)
