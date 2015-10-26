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

#ifdef HAVE_BOOST
#ifdef HAVE_OPENSSL

#include "Common.h"
#include "util/AsioHttp.h"
#include "util/IO.h"
#include "util/Flickr.h"
#include <CppAsync/AsioWrappers.h>
#include <CppAsync/Combinators.h>
#include <CppAsync/StacklessAsync.h>
#include <CppAsync/util/Range.h>
#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <deque>
#include <fstream>

namespace asio = boost::asio;
using asio::ip::tcp;

static asio::io_service sIo;

namespace {

using namespace util::flickr;

//
// Parallel downloader
//

static ut::Task<void> asyncFlickrDownload(const std::vector<std::string>& tags,
    int numPics, int numPicsPerPage)
{
    static const int MAX_PARALLEL_DOWNLOADS = 4;

    struct Frame : ut::AsyncFrame<void>
    {
        Frame(std::vector<std::string> tags, int numPics, int numPicsPerPage)
            : tags(std::move(tags))
            , numPics(numPics)
            , numPicsPerPage(numPicsPerPage)
            , ctx(ut::makeContext<Context>()) { }

        void operator()()
        {
            ut::AwaitableBase *doneTask = nullptr;
            ut_begin();

            // Prepare a persistent SSL connection for API queries.
            connectTask = util::asyncHttpsClientConnect(ctx->apiSocket, ctx, FLICKR_API_HOST);
            ut_await_(connectTask);
            ctx->apiSocket.lowest_layer().set_option(asio::socket_base::keep_alive(true));

            totalPicsRemaining = numPics;
            page = 1;
            numActiveTransfers = 0;

            while (totalPicsRemaining > 0 || numActiveTransfers > 0) {
                if (!ctx->querySlot.task.isRunning()
                    && (photoQueue.size() < (size_t) std::min(numPics / 2, totalPicsRemaining))) {
                    // Request a page of photo links.
                    auto queryUrl = makeFlickrQueryUrl(tags, numPicsPerPage, page++);

                    printf("-> starting query (got %d photos in queue, need %d)...\n",
                        (int) photoQueue.size(), totalPicsRemaining);

                    ctx->querySlot.task = util::asyncHttpGet(ctx->apiSocket, ctx->querySlot.buf,
                        ctx, queryUrl.host, queryUrl.path, true);
                    numActiveTransfers++;
                }

                for (auto& dlSlot : ctx->dlSlots) {
                    if (totalPicsRemaining == 0 || photoQueue.empty())
                        break;

                    if (!dlSlot.task.isRunning()) {
                        // Start downloading a photo from the queue.
                        dlSlot.photo = std::move(photoQueue.front());
                        photoQueue.pop_front();
                        auto photoUrl = makeFlickrPhotoUrl(dlSlot.photo);

                        printf("-> [%d] downloading %s%s...\n",
                            ctx->indexOf(dlSlot), photoUrl.host.c_str(), photoUrl.path.c_str());

                        dlSlot.task = util::asyncHttpDownload(sIo, dlSlot.buf, ctx,
                            photoUrl.host, photoUrl.path);
                        numActiveTransfers++;
                        totalPicsRemaining--;
                    }
                }

                anyDownloadTask = ut::whenAny(ctx->dlSlots);

                // Suspend until an API query or photo download finishes.
                ut_await_any_(doneTask, ctx->querySlot.task, anyDownloadTask);
                numActiveTransfers--;

                if (doneTask == &ctx->querySlot.task) { // query done
                    // Cancel whenAny combinator. Resets awaiter to nullptr for all download tasks.
                    anyDownloadTask.cancel();

                    FlickrPhotos resp = parseFlickrResponse(ctx->querySlot.buf);

                    printf("<- query result: %ld photos, page %d/%d, %d per page, %d total\n",
                        (long) resp.photos.size(), resp.page, resp.pages, resp.perPage, resp.total);

                    int availablePicsRemaining = resp.total - (resp.page - 1) * resp.perPage;
                    totalPicsRemaining = std::min(totalPicsRemaining, availablePicsRemaining);

                    for (auto& photo : resp.photos)
                        photoQueue.push_back(std::move(photo));

                    printf("   photo queue size: %d\n", (int) photoQueue.size());

                    // Make slot available.
                    ctx->querySlot.task = ut::Task<size_t>();
                } else { // photo download done
                    auto& dlSlot = *anyDownloadTask.get();

                    if (dlSlot.task.hasError())
                        std::rethrow_exception(dlSlot.task.error());

                    std::string savePath = dlSlot.photo.id + ".jpg";
                    std::ofstream fout(savePath, std::ios::binary);
                    fout << &dlSlot.buf;
                    printf("<- [%d] photo downloaded (%s)\n", ctx->indexOf(dlSlot),
                        savePath.c_str());

                    // Make slot available.
                    dlSlot.task = ut::Task<size_t>();
                }
            }

            ut_end();
        }

    private:
        struct TransferSlot
        {
            asio::streambuf buf;
            ut::Task<size_t> task;
        };

        struct DownloadSlot : TransferSlot
        {
            FlickrPhoto photo;
        };

        struct Context
        {
            asio::ssl::context sslContext;
            asio::ssl::stream<tcp::socket> apiSocket;
            DownloadSlot dlSlots[MAX_PARALLEL_DOWNLOADS];
            TransferSlot querySlot;

            Context()
                : sslContext(asio::ssl::context::tlsv12_client)
                , apiSocket(sIo, sslContext) { }

            int indexOf(DownloadSlot& dlSlot) const
            {
                for (int i = 0; i < MAX_PARALLEL_DOWNLOADS; i++) {
                    if (&dlSlot == dlSlots + i)
                        return i;
                }
                return -1;
            }
        };

        const std::vector<std::string> tags;
        const int numPics;
        const int numPicsPerPage;
        ut::ContextRef<Context> ctx;
        int totalPicsRemaining;
        int page;
        int numActiveTransfers;
        std::deque<FlickrPhoto> photoQueue;
        ut::Task<tcp::endpoint> connectTask;
        ut::Task<DownloadSlot*> anyDownloadTask;
    };

    return ut::startAsyncOf<Frame>(std::move(tags), numPics, numPicsPerPage);
}

}

void ex_flickr()
{
    printf ("Tags (default 'kitten'): ");
    std::string tags = util::readLine();

    std::vector<std::string> splitTags;
    boost::split(splitTags, tags, boost::is_space());
    splitTags.resize(
        std::remove(splitTags.begin(), splitTags.end(), "") - splitTags.begin());

    if (splitTags.empty())
        splitTags.push_back("kitten");

    ut::Task<void> task = asyncFlickrDownload(splitTags, 25, 10);

    sIo.run();

    assert(task.isReady());
    try {
        task.get();
    } catch (std::exception& e) {
        printf ("Flickr download failed: %s - %s\n", typeid(e).name(), e.what());
    } catch (...) {
        printf ("Flickr download failed: unknown exception\n");
    }
}

#endif // HAVE_OPENSSL
#endif // HAVE_BOOST
