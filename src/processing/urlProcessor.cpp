#include "urlProcessor.h"
#include <curl/curl.h>
#include <unordered_set>
#include <vector>

std::vector<std::string> UrlProcessor::process(
    const std::vector<std::string>& links,
    const std::string& baseUrl
) const {
    return links;
}