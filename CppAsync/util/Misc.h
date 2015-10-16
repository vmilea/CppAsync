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

#include "../impl/Common.h"
#include "Meta.h"

namespace ut {

//
// Misc utils
//

template <class T>
void genericReset(T& resource) _ut_noexcept
{
    static_assert(std::is_nothrow_default_constructible<T>::value,
        "May reset only move-assignable types with a no-throw default constructor");

    static_assert(std::is_move_assignable<T>::value,
        "May reset only move-assignable types with a no-throw default constructor");

    resource = T();
}

}
