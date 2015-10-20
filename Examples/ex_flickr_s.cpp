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

#ifdef HAVE_BOOST_CONTEXT
#ifdef HAVE_OPENSSL

#include "Common.h"
#include "util/AsioHttp.h"
#include "util/IO.h"
#include "util/Flickr.h"
#include <CppAsync/AsioWrappers.h>
#include <CppAsync/Combinators.h>
#include <CppAsync/StackfulAsync.h>
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

    struct TransferSlot
    {
        asio::streambuf buf;
        ut::Task<size_t> task;

        TransferSlot()
            : task(ut::makeTask<size_t>()) { }
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

    return ut::stackful::startAsync([tags, numPics, numPicsPerPage](
        ut::stackful::Context<void> coroContext) {
        auto ctx = ut::makeContext<Context>();

        // Prepare a persistent SSL connection for API queries.
        ut::Task<tcp::endpoint> connectTask = util::asyncHttpsClientConnect(ctx->apiSocket, ctx,
            FLICKR_API_HOST);
        ut::stackful::await_(connectTask);
        ctx->apiSocket.lowest_layer().set_option(asio::socket_base::keep_alive(true));

        int totalPicsRemaining = numPics;
        int page = 1;
        int numActiveTransfers = 0;
        std::deque<FlickrPhoto> photoQueue;

        while (totalPicsRemaining > 0 || numActiveTransfers > 0) {
            if (!ctx->querySlot.task.isRunning()
                && (photoQueue.size() < (size_t) std::min(numPics / 2, totalPicsRemaining))) {
                // Request a page of photo links.
                auto queryUrl = makeFlickrQueryUrl(tags, numPicsPerPage, page++);

                printf("-> starting query (got %d photos in queue, need %d)...\n",
                    (int) photoQueue.size(), totalPicsRemaining);

                ctx->querySlot.task = util::asyncHttpGet(ctx->apiSocket, ctx->querySlot.buf, ctx,
                    queryUrl.host, queryUrl.path, true);
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

            auto anyDownloadTask = ut::whenAny(ctx->dlSlots);

            // Suspend until an API query or photo download finishes.
            auto *doneTask = ut::stackful::awaitAny_(ctx->querySlot.task, anyDownloadTask);
            numActiveTransfers--;

            if (doneTask == &ctx->querySlot.task) { // query done
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
                printf("<- [%d] photo downloaded (%s)\n", ctx->indexOf(dlSlot), savePath.c_str());

                // Make slot available.
                dlSlot.task = ut::Task<size_t>();
            }
        }
    }, 256 * 1024); // need stack > 64KB
}

}

void ex_flickr_s()
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
#endif // HAVE_BOOST_CONTEXT
