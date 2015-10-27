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
#include "util/IO.h"

struct Example
{
    void (*function)();
    const char *description;
};

void ex_fibo();
void ex_countdown();
void ex_abortableCountdown();
#ifdef HAVE_BOOST
void ex_http();
#ifdef HAVE_OPENSSL
void ex_flickr();
#endif
void ex_chatServer();
void ex_chatClient();
#endif // HAVE_BOOST
void ex_customAwaitable();

#ifdef HAVE_BOOST_CONTEXT
void ex_fibo_s();
void ex_countdown_s();
void ex_abortableCountdown_s();
void ex_http_s();
#ifdef HAVE_OPENSSL
void ex_flickr_s();
#endif
void ex_chatServer_s();
void ex_chatClient_s();
void ex_customAwaitable_s();
#endif // HAVE_BOOST_CONTEXT

#if defined(_MSC_VER) && _MSC_VER >= 1900
void ex_fibo_n4402();
void ex_countdown_n4402();
#endif

static const Example EXAMPLES[] =
{
    { &ex_fibo,                 "coro  - Fibonacci generator" },
    { &ex_countdown,            "async - countdown" },
    { &ex_abortableCountdown,   "async - abortable countdown" },
#ifdef HAVE_BOOST
    { &ex_http,                 "async - HTTP download" },
    { &ex_chatServer,           "async - chat server" },
    { &ex_chatClient,           "async - chat client" },
#ifdef HAVE_OPENSSL
    { &ex_flickr,               "async - Flickr client" },
#endif
#endif // HAVE_BOOST
    { &ex_customAwaitable,      "async - custom awaitable" },

#ifdef HAVE_BOOST_CONTEXT
    { &ex_fibo_s,               "coro  (stackful) - Fibonacci generator" },
    { &ex_countdown_s,          "async (stackful) - countdown" },
    { &ex_abortableCountdown_s, "async (stackful) - abortable countdown" },
    { &ex_http_s,               "async (stackful) - HTTP download" },
    { &ex_chatServer_s,         "async (stackful) - chat server" },
    { &ex_chatClient_s,         "async (stackful) - chat client" },
#ifdef HAVE_OPENSSL
    { &ex_flickr_s,             "async (stackful) - Flickr client" },
#endif
    { &ex_customAwaitable_s,    "async (stackful) - custom awaitable" },
#endif // HAVE_BOOST_CONTEXT

#if defined(_MSC_VER) && _MSC_VER >= 1900
    { &ex_fibo_n4402,           "coro  (C++17 resumable functions) - Fibonacci "
                                "generator" },
    { &ex_countdown_n4402,      "async (C++17 resumable functions) - countdown" },
#endif
};

int main()
{
    std::size_t numExamples = sizeof(EXAMPLES) / sizeof(Example);
    std::size_t selected = 0;

    while (selected < 1 || numExamples < selected) {
        printf("Examples:\n\n");

        for (std::size_t i = 0; i < numExamples; i++)
            printf("%02d: %s\n", (int) (i + 1), EXAMPLES[i].description);

        printf("\nChoose: ");
        try {
            const char *line = util::readLine();
            selected = util::lexicalCast<std::size_t>(line);
        } catch (...) {
            printf("cast error!\n");
        }

        printf("\n----------\n\n");
    }

    auto example = EXAMPLES[selected - 1];
    example.function();

    printf("\nDONE\n");

    return 0;
}
