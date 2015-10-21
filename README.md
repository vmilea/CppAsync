#CppAsync

[*WARNING:* CppAsync is a work in progress -- it's usable, but expect some bugs, unstable API, and poor documentation.]


## Bullet points

* Portable C++11 library
* Enables async/await pattern.
* Helps you write clean, efficient, composable async code.
* Await user defined types, like custom Futures!
* Provides asymmetrical coroutines.
* Can use several coroutine back-ends (Duff / Boost.Context / C++17 resumable functions).
* Works with any kind of event loop (Qt / Boost.Asio / libuv etc.)
* Header only, zero external dependencies (optional stackful coroutines via Boost.Context)
* Easy to understand samples
* Full support for exceptions in coroutines
* Efficient implementation
* Custom allocators support
* Applicable even where exceptions are prohibited (embedded friendly)
* Boost.Asio wrappers for convenience


## Pitch

Couroutines are making a comeback. The increasing adoption of asynchronous APIs has been plagued by ever more obscure control flow -- the so-called 'callback hell'. Coroutines can be used to bring back the order of structured programming: plain `if` / `else` / `for` / `do` / `while` statements, scoped life-time, and exception handling instead of what might otherwise be incomprehensible chains of callbacks.

Intuitively, coroutines are just functions that may be suspended and then resumed. The function's state must be preserved while it is suspended, either on the heap (for so-called stackless coroutines), or on a separate stack (stackful coroutines). Suspension and resumal are cooperative -- there is no preemptive scheduling by the kernel. However, your application can easily coordinate thousands of coroutines on top of a single kernel thread.

This ability to yield to one another and be resumed, combined with ease of use and inherent efficiency, makes coroutines great for writing generators and coordinating async tasks.

Languages like C#, Phython, JavaScript, Dart, and Lua already support coroutines in one form or another via `yield` or `async` / `await` operators. For C++ there is proposal [N4403 - Resumable Functions](https://isocpp.org/files/papers/N4403.pdf) (a kind of stackless coroutines), which might become part of C++17. Here is where CppAsync comes in: it makes these constructs available to you right now in portable C++11, with an easy migration path to the baked-in version once it becomes supported by your target compilers.


## Overview

CppAsync has various applications (network or local I/O, responsive UI development, coarse-grained parallelism) and is designed to scale from high traffic web-servers all the way down to embedded systems that need custom memory allocation and prohibit exceptions.

### Coroutines

The library builds on top of a coroutine layer without being tied to any particular back-end. It supports:
- Stackless coroutines based on [Duff's device](https://en.wikipedia.org/wiki/Duff%27s_device). They are 100% portable and have minimal overhead, but are somewhat clunky to write.
- Stackful coroutines on top of Boost.Context. They are supported on [common architectures](http://www.boost.org/doc/libs/1_59_0/libs/context/doc/html/context/architectures.html), fast, simple to write, but harder to debug. For each stackful coroutine at least 4KB of address space has to be reserved.
- Resumable functions as proposed in N4403. They are similar to the first back-end, with the compiler doing all the heavy lifting instead. There is preliminary support in Visual Studio 2015 (with exception handling notably left out).

Your application can use different kinds of coroutines under the common `ut::Coroutine` wrapper. You might start with a stackful implementation, then switch to stackless for efficiency.

See helper function `ut::makeCoroutine()` for turning functors into coroutines.

_Table 1. Creating coroutines on top of supported backends:_

<table style="width:100%">
  <tr align="center">
    <td colspan="2">makeCoroutine(...) -> Coroutine<br>(CppAsync helper)</td>
    <td>Coroutine foo() { ... yield x; ... }<br>(compiler magic)</td> 
  </tr>
  <tr align="center">
    <td>StacklessCoroutine</td>
    <td>StackfulCoroutine</td> 
    <td>coroutine_traits&lt;Coroutine&gt;</td>
  </tr>
  <tr align="center">
    <td>Duff's device</td>
    <td>Boost.Context</td>
    <td>N4403 compiler</td>
  </tr>
</table>

#### Yield operator

The coroutines in CppAsync are asymmetric: control flows from the caller context to the coroutine context and back. This means suspending a coroutine is always done by yielding control back to its parent.

Each of the three coroutine back-ends has its own version of the `yield` operator. Please refer to their respective documentation.

### Awaitables

_Awaitable_ is a concept for some operation whose result might not be immediately available. It supports registration of an `Awaiter` (usually the owner of the operation) which will be notified on completion. Such an operation may either finish successfully and produce a result, or fail with an error.

For example, futures fit this description. So you could take `boost::future<R>` (which supports continuation), specialize `ut::AwaitableTraits`, and voilà, await operator works with Boost futures!

_Table 2. Awaitable concept [(*)](#awt-traits). Given an awaitable object `awt`, and `ut::Awaiter*` awaiter:_

 Expression                | Return type | Description                                      
---------------------------|-------------|--------------------------------------------------
 `awt.isReady()`           | `bool`      | Check if `awt` has result or error.              
 `awt.hasError()`          | `bool`      | Check if `awt` has error.                        
 `awt.setAwaiter(awaiter)` | `void`      | Set completion handler, to be called when ready. 
 `awt.takeResult()`        | `R`         | Pop result out of `awt`.                         
 `awt.takeError()`         | `Error`     | Pop error out of `awt`.                          

<a id="awt-traits">(*)</a>  Types that don't conform precisely to this definition may be adapted through `ut::AwaitableTraits<T>`.

While awaitables are trait based, `Awaiter`s must derive from a common interface:

```c++
struct Awaiter {
    virtual void resume(void *resumer) = 0;
};
```

CppAsync provides a lightweight awaitable type called `ut::Task<R>` as default. Tasks automatically manage resources and cancellation policy for their underlying operation.

### Async coroutines

Awaitables are not intrinsically tied to coroutines. But coroutines serve well for coordinating and combining async operations. If only they implemented the `Awaiter` interface! The helper function `startAsync()` adds the necessary glue and exposes the coroutine itself as an awaitable `Task<R>`. This makes it possible to compose async operations as easily as regular functions.

_Table 3. Creating async coroutines:_

<table style="width:100%">
  <tr align="center">
    <td colspan="2">startAsync(...) -> Task&lt;R&gt;<br>(CppAsync helper)</td>
    <td>Task&lt;R&gt; foo() { ... await x; ...}<br>(compiler magic)</td> 
  </tr>
  <tr align="center">
    <td>StacklessCoroutine</td>
    <td>StackfulCoroutine</td> 
    <td>coroutine_traits&lt;Task&lt;R&gt;&gt;</td>
  </tr>
  <tr align="center">
    <td>Duff's device</td>
    <td>Boost.Context</td>
    <td>N4403 compiler</td>
  </tr>
</table>

#### Await operators

The `await` operator may be used from within an async coroutine. If the awaited object is not already done, the coroutine registers as `Awaiter` then suspends itself. Eventually, the awaited operation completes and the coroutine gets resumed.

Await operators come in several flavors like `await`, `awaitNoThrow`, `awaitAll`, or `awaitAny`, and differ depending on back-end. Please refer to their respective documentation.

#### Async coroutines in practice

Here is a snippet of typical library use. The stackful coroutine below resolves a host name, then tries each endpoint until a connection is established:

```c++
ut::Task<tcp::endpoint> asyncResolveAndConnect(
    tcp::socket& socket,
    tcp::resolver::query query)
{
    using namespace ut::stackful;

    return startAsync([&socket, query]() -> tcp::endpoint {
        tcp::resolver resolver(socket.get_io_service());

        auto resolveTask = asyncResolve(resolver, query);

        // Suspends coroutine until task has finished.
        auto it = await_(resolveTask);

        for (; it != tcp::resolver::iterator(); ++it) {
            tcp::endpoint ep = *it;
            auto connectTask = asyncConnect(socket, ep);

            try {
                // Suspends coroutine until task has finished.
                await_(connectTask);

                return ep;
            } catch (...) {
                // Try next endpoint.
            }
        }

        throw SocketError("Failed to connect socket");
    });
}
```

Below is the same task implemented over a stackless coroutine. Variables that need to be persisted across suspension points are stored as fields in an `AsyncFrame`. The frame gets allocated on the heap (custom allocators are also supported), and deallocated once the task completes or is canceled.

```c++
ut::Task<tcp::endpoint> asyncResolveAndConnect(
    tcp::socket& socket, tcp::resolver::query query)
{
    struct Frame : ut::AsyncFrame<tcp::endpoint>
    {
        Frame(tcp::socket& socket, tcp::resolver::query query)
            : socket(socket)
            , query(query)
            , resolver(socket.get_io_service()) { }

        void operator()()
        {
            // Body must be wrapped betweeen ut_begin() .. ut_end() macros.
            ut_begin();

            resolveTask = asyncResolve(resolver, query);

            // Suspends coroutine until task has finished.
            ut_await_(resolveTask);

            for (it = resolveTask.get(); it != tcp::resolver::iterator(); ++it) {
                connectTask = asyncConnect(socket, *it);

                ut_try {
                    // Suspends coroutine until task has finished.
                    ut_await_(connectTask);

                    ut_return(*it);
                } ut_catch (...) {
                    // Try next endpoint.
                }
            }

            throw SocketError("Failed to connect socket");
            ut_end();
        }

    private:
        tcp::socket& socket;
        tcp::resolver::query query;
        tcp::resolver resolver;
        tcp::resolver::iterator it;
        ut::Task<tcp::resolver::iterator> resolveTask;
        ut::Task<void> connectTask;
    };

    return ut::startAsync<Frame>(socket, query);
}
```

## Adding CppAsync to your project

Just add CppAsync to the include path of your project. There are no mandatory [(*)](#linking-boost-context) dependencies.

The library is single threaded, and relies on your application having some kind of event loop (Qt / Boost.Asio / libuv etc.) on top of which async coroutines are going to run and be resumed. Please see examples on how to integrate with Boost.Asio or a custom event loop.

<a id="linking-boost-context">(*)</a> The Boost.Context library must be linked if using stackful coroutines.

## Building the examples

CppAsync bundles an `Examples` project. To compile everything it's recommended to have Boost and OpenSSL libraries installed.

Each sample comes in both stackful and stackless version. If Boost.Context is missing, the stackful versions will be skipped. If the Boost library is missing altogether, then Asio examples will be skipped as well. One of the samples uses Flickr API, and will be skipped if OpenSSL is not found.

### Quick guide:

1. Install [Boost](http://www.boost.org/users/download/) 1.58 or later. Either use [pre-built binaries](http://sourceforge.net/projects/boost/files/boost-binaries/), or build Boost.Context and Boost.Thread manually:

   - download and unpack Boost archive
   - `./bootstrap`
   - `./b2 link=static --build-type=minimal --with-context --with-thread --toolset=your-toolset stage`

2. Install [OpenSSL](https://www.openssl.org/source/). Use [pre-built binaries](https://www.openssl.org/community/binaries.html) or build yourself.

3. Install [CMake](http://www.cmake.org/cmake/resources/software.html) 3.1 or later.

4. Build CppAsync:

   - `mkdir build_dir ; cd build_dir`
   - `cmake -G"your-generator" -DBOOST_ROOT="path-to-boost" -DOPENSSL_ROOT_DIR="path-to-openssl" "path-to-cppasync"`
   - `make` / open solution


## Portability

CppAsync should work with any reasonable C++11 compiler. It has been tested on:

   - MSVC 12.0 (Visual Studio 2013 Update 5)
   - MSVC 14.0 (Visual Studio 2015)
   - GCC 4.9.2
   - GCC 5.2.0
   - Clang 3.7.0

Optional stackful coroutines are provided by Boost.Context and supported on [common architectures](http://www.boost.org/doc/libs/1_59_0/libs/context/doc/html/context/architectures.html).


## Authors

Valentin Milea <valentin.milea@gmail.com>


## License

    Copyright 2012-2015 Valentin Milea

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
