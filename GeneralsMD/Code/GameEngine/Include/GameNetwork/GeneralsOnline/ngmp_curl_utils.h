// GeneralsX @feature GeneralsOnline Internal libcurl write callback utilities
// Shared by OnlineServices_Auth.cpp and OnlineServices_Manager.cpp to avoid code duplication.

#pragma once
#include <limits>
#include <string>
#include <curl/curl.h>

namespace NGMP {
namespace Internal {

struct CurlResponse {
    std::string text;
    size_t maxBytes = (std::numeric_limits<size_t>::max)();
};

inline size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    CurlResponse* resp = static_cast<CurlResponse*>(userp);
    if (!size || resp->text.size() > resp->maxBytes || nmemb > (resp->maxBytes - resp->text.size()) / size)
        return 0;
    const size_t totalSize = size * nmemb;
    resp->text.append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

} // namespace Internal
} // namespace NGMP
