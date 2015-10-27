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

/** See: http://www.boost.org/doc/libs/develop/doc/html/boost_asio/example/cpp11/chat/ */

#pragma once

#include "Common.h"
#include <CppAsync/util/StringUtil.h>
#include <cstdio>
#include <deque>
#include <set>

namespace {

// Single line message
//
using Msg = std::string;

// Chat guest interface
//
class Guest
{
public:
    virtual ~Guest() { }

    virtual const char* nickname() = 0;

    virtual void push(const Msg& msg) = 0;
};

// Chat room handles guest list and message dispatch.
//
class ChatRoom
{
public:
    void add(Guest *guest)
    {
        mGuests.insert(guest);

        // Enqueue recent messages.
        for (auto& msg : mHistory)
            guest->push(msg);

        auto line = ut::string_printf("%s has joined", guest->nickname());
        printf("%s\n", line.c_str());

        // Notify all clients.
        broadcast(":server", line);
    }

    void remove(Guest *guest)
    {
        auto pos = mGuests.find(guest);

        if (pos != mGuests.end()) {
            mGuests.erase(pos);

            auto line = ut::string_printf("%s has left", guest->nickname());
            printf("%s\n", line.c_str());

            // Notify all clients.
            broadcast(":server", line);
        }
    }

    void broadcast(const std::string& sender, const std::string& line)
    {
        Msg msg = ut::string_printf("%s: %s\n", sender.c_str(), line.c_str());

        for (auto *guest : mGuests) {
            guest->push(msg);
        }

        if (MAX_HISTORY_SIZE > 0) {
            if (mHistory.size() == MAX_HISTORY_SIZE)
                mHistory.pop_front();

            mHistory.push_back(msg);
        }
    }

private:
    static const std::size_t MAX_HISTORY_SIZE = 10;

    std::set<Guest*> mGuests;
    std::deque<Msg> mHistory; // circular buffer
};

}
