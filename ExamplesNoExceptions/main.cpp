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

#include "Common.h"
#include "../Examples/util/IO.h"

struct Example
{
    void (*function)();
    const char *description;
};

void ex_fibo();
void ex_countdown();

static const Example EXAMPLES[] =
{
    { &ex_fibo,                 "coro  - Fibonacci generator (no exceptions)" },
    { &ex_countdown,            "async - countdown (no exceptions)" },
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
        const char *line = util::readLine();
        selected = util::lexicalCast<std::size_t>(line);

        printf("\n----------\n\n");
    }

    auto example = EXAMPLES[selected - 1];
    example.function();

    printf("\nDONE\n");

    return 0;
}
