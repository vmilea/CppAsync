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

#include "Common.h"
#include "IO.h"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace util {

template <class T>
T lexicalCastImpl(const std::string& s)
{
    std::stringstream ss;
    ss << s;
    T value;
    ss >> value;

    if (ss.fail())
        throw std::invalid_argument("Lexical cast failed");

    return value;
}

template <>
size_t lexicalCast<size_t>(const std::string& s)
{
    return lexicalCastImpl<size_t>(s);
}

template <>
int lexicalCast<int>(const std::string& s)
{
    return lexicalCastImpl<int>(s);
}

const char* readLine()
{
    static char sLine[512];

    void* result = fgets(sLine, sizeof(sLine), stdin);

    if (result == nullptr) {
        sLine[0] = '\0';
    } else {
        sLine[strlen(sLine) - 1] = '\0';
    }

    return sLine;
}

}
