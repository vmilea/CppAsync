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

#pragma once

#ifdef HAVE_BOOST

#include "../Common.h"
#include <boost/asio/streambuf.hpp>
#include <string>
#include <vector>

namespace util { namespace flickr {

static const std::string FLICKR_API_HOST = "api.flickr.com";
static const std::string FLICKR_API_KEY = "e36784df8a03fea04c22ed93318b291c";

struct FlickrPhoto
{
    std::string id;
    std::string owner;
    std::string secret;
    std::string server;
    std::string farm;
    std::string title;
};

struct FlickrPhotos
{
    int page;
    int pages;
    int perPage;
    int total;
    std::vector<FlickrPhoto> photos;
};

struct Url
{
    std::string host;
    std::string path;
};

Url makeFlickrQueryUrl(const std::vector<std::string>& tags, int perPage, int page);

Url makeFlickrPhotoUrl(const FlickrPhoto& photo);

FlickrPhotos parseFlickrResponse(boost::asio::streambuf& response);

} } // util::flickr

#endif // HAVE_BOOST
