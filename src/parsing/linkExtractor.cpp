#include "parsing/linkExtractor.h"
#include "parsing/urlParser.h"

std::vector<std::string> extract_links(
    const std::string& html,
    const std::string& base_url
) {
    std::vector<std::string> links;
    std::string marker = "href=\"";

    size_t pos = 0;
    while ((pos = html.find(marker, pos)) != std::string::npos) {
        size_t start = pos + marker.size();
        size_t end = html.find('"', start);

        if (end == std::string::npos)
            break;

        std::string link = html.substr(start, end - start);

        // Resolve relative URLs and validate the scheme
        auto resolved = resolveUrl(base_url, link);

        if (resolved.has_value()) {
            links.push_back(*resolved);
        }

        pos = end;
    }

    return links;
}