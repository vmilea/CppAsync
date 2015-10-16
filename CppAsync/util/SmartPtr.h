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
#include <memory>

namespace ut {

//
// Extensions for unique_ptr
//

template <class T, class ...Args>
std::unique_ptr<T, std::default_delete<T>> makeUnique(Args&&... args)
{
    return std::unique_ptr<T>(new T(
        std::forward<Args>(args)...));
}

template <class T>
std::unique_ptr<T, std::default_delete<T>> asUnique(T *value) _ut_noexcept
{
    return std::unique_ptr<T>(value);
}

//
// Move semantics for regular pointers
//

template <class T>
T* movePtr(T*& value) _ut_noexcept
{
    T *moved = value;
    value = nullptr;
    return moved;
}

}
