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

/**
 * Uncomment to strip all logging code from build. Currently has no effect.
 */
// #define UT_DISABLE_LOGGING

/**
 * Uncomment to use library in environments where exceptions are prohibited.
 */
// #define UT_DISABLE_EXCEPTIONS

/**
 * Define error type when exceptions are disabled
 */
#define UT_CUSTOM_ERROR_TYPE int32_t

/**
 * Maximum supported depth for stackful coroutines
 */
#define UT_MAX_COROUTINE_DEPTH 16
