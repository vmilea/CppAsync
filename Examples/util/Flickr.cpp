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

#include "../Common.h"
#include "Flickr.h"
#include <CppAsync/util/StringUtil.h>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <sstream>
#include <stdexcept>

namespace util { namespace flickr  {

Url makeFlickrQueryUrl(const std::vector<std::string>& tags, int perPage, int page)
{
    std::string path = "/services/rest/?method=flickr.photos.search&format=rest&api_key="
        + FLICKR_API_KEY;

    path += "&tags=" + tags[0];
    for (size_t i = 1; i < tags.size(); i++)
        path += "+" + tags[i];

    path += "&per_page=" + boost::lexical_cast<std::string>(perPage);
    path += "&page=" + boost::lexical_cast<std::string>(page);

    return Url { FLICKR_API_HOST, path };
}

Url makeFlickrPhotoUrl(const FlickrPhoto& photo)
{
    // Format: http://farm{farm-id}.staticflickr.com/{server-id}/{id}_{secret}_[mstzb].jpg

    std::string host = "farm" + photo.farm + ".staticflickr.com";
    std::string path = "/" + photo.server + "/" + photo.id + "_" + photo.secret + "_m.jpg";

    return Url { host, path };
}

FlickrPhotos parseFlickrResponse(boost::asio::streambuf& response)
{
    namespace pt = boost::property_tree;

    FlickrPhotos result;

    std::stringstream ss;
    ss << &response;
    pt::ptree tree;
    pt::read_xml(ss, tree);

    const pt::ptree& rsp = tree.get_child("rsp");
    const std::string& stat = rsp.get("<xmlattr>.stat", "error");

    if (stat != "ok")
        throw std::runtime_error(ut::string_printf(
            "Flickr response not ok: %s", stat.c_str()));

    for (auto& node : rsp.get_child("photos")) {
        const pt::ptree& value = node.second;

        if (node.first == "<xmlattr>") {
            result.page = value.get<int>("page");
            result.pages = value.get<int>("pages");
            result.perPage = value.get<int>("perpage");
            result.total = value.get<int>("total");
        } else {
            FlickrPhoto fp;
            fp.id = value.get<std::string>("<xmlattr>.id");
            fp.owner = value.get<std::string>("<xmlattr>.owner");
            fp.secret = value.get<std::string>("<xmlattr>.secret");
            fp.server = value.get<std::string>("<xmlattr>.server");
            fp.farm = value.get<std::string>("<xmlattr>.farm");
            fp.title = value.get<std::string>("<xmlattr>.title");

            result.photos.push_back(fp);
        }
    }

    return result;
}

} } // util::flickr

#endif // HAVE_BOOST
