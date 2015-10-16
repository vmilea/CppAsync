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

#ifdef _MSC_VER

#ifndef WINVER
#define WINVER 0x501
#endif

#ifndef _WIN32_WINNT
#define _WIN32_WINNT WINVER
#endif

#ifdef NDEBUG
#define _SECURE_SCL 0
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#define WIN32_LEAN_AND_MEAN

#endif // _MSC_VER

#include "../CppAsync/impl/Common.h"

#ifdef UT_DISABLE_EXCEPTIONS

#define BOOST_NO_EXCEPTIONS

namespace boost
{
    inline void throw_exception(std::exception const & e)
    {
        std::terminate();
    }
}

#endif // UT_DISABLE_EXCEPTIONS
