#include "parsing/linkExtractor.h"

std::vector<std::string> extract_links(const std::string& html, const std::string& base_url) {
    std::vector<std::string> links;
    std::string marker = "href=\"";

    size_t pos = 0;
    while ((pos = html.find(marker, pos)) != std::string::npos) {
        size_t start = pos + marker.size();
        size_t end = html.find('"', start);

        if (end == std::string::npos)
            break;

        std::string link = html.substr(start, end - start);

        // only keep http/https links for now — skip mailto:, #anchors, javascript:, relative paths
        if (link.rfind("http://", 0) == 0 || link.rfind("https://", 0) == 0) {
            links.push_back(link);
        }

        pos = end;
    }

    return links;
}