#include "urlProcessor.h"
#include <curl/curl.h>
#include <unordered_set>
#include <vector>
#include <lexbor/url/url.h>
#include <algorithm>
#include <cctype>

static lxb_status_t appendUrl(
    const lxb_char_t* data,
    size_t len,
    void* ctx
) {
    auto* result = static_cast<std::string*>(ctx);
    result->append(reinterpret_cast<const char*>(data), len);
    return LXB_STATUS_OK;
}

std::vector<std::string> UrlProcessor::resolveRelative(
    const std::vector<std::string>& links,
    const std::string& baseUrl
) const {
    std::vector<std::string> resolved;
    resolved.reserve(links.size());

    lxb_url_parser_t parser;

    if (lxb_url_parser_init(&parser, nullptr) != LXB_STATUS_OK) {
        return resolved;
    }

    auto* base = lxb_url_parse(
        &parser, nullptr,
        reinterpret_cast<const lxb_char_t*>(baseUrl.data()),
        baseUrl.size()
    );

    if (base == nullptr) {
        lxb_url_parser_destroy(&parser, false);
        return resolved;
    }

    for (const auto& link : links) {
        lxb_url_parser_clean(&parser);

        auto* url = lxb_url_parse(
            &parser, base,
            reinterpret_cast<const lxb_char_t*>(link.data()),
            link.size()
        );

        if (url == nullptr) {
            continue;
        }

        std::string absoluteUrl;

        if (lxb_url_serialize(
            url, appendUrl, &absoluteUrl, false
        ) == LXB_STATUS_OK) {
            resolved.push_back(std::move(absoluteUrl));
        }
    }

    lxb_url_parser_destroy(&parser, false);
    lxb_url_parser_memory_destroy(&parser);

    return resolved;
}

std::vector<std::string> UrlProcessor::removeFragments(
    const std::vector<std::string>& links
) const {
    std::vector<std::string> result;
    result.reserve(links.size());

    for (const auto& link : links) {
        const auto pos = link.find('#');

        if (pos == std::string::npos) {
            result.push_back(link);
        }
        else {
            result.push_back(link.substr(0, pos));
        }
    }

    return result;
}

std::vector<std::string> UrlProcessor::deduplicateLinks(const std::vector<std::string>& links) const
{
    std::vector<std::string> deduplicatedLinks;
    std::unordered_set<std::string> seen;

    for (const std::string& url : links) {
        if (!seen.count(url)) {
            seen.insert(url);
            deduplicatedLinks.push_back(url);
        }
    }

    return deduplicatedLinks;
}

std::vector<std::string> UrlProcessor::filterLinks(
    const std::vector<std::string>& links
) const {
    static const std::unordered_set<std::string> excludedExtensions = {
        ".jpg", ".jpeg", ".png", ".gif", ".webp", ".svg", ".ico",
        ".pdf",
        ".js", ".css",
        ".mp4", ".mp3", ".wav", ".avi", ".mov",
        ".zip", ".rar", ".7z", ".exe",
        ".doc", ".docx", ".ppt", ".pptx", ".xls", ".xlsx"
    };

    std::vector<std::string> filtered;
    filtered.reserve(links.size());

    for (const auto& url : links) {
        // Only allow HTTP and HTTPS.
        if (url.rfind("http://", 0) != 0 &&
            url.rfind("https://", 0) != 0) {
            continue;
        }

        // Ignore query parameters and fragments.
        const auto end = url.find_first_of("?#");
        const std::string cleanUrl = url.substr(0, end);

        // Find the final path segment.
        const auto schemeEnd = cleanUrl.find("://");
        const auto pathStart = cleanUrl.find('/', schemeEnd + 3);

        if (pathStart != std::string::npos) {
            const auto slash = cleanUrl.find_last_of('/');
            const auto dot = cleanUrl.find_last_of('.');

            if (dot != std::string::npos && dot > slash) {
                std::string extension = cleanUrl.substr(dot);

                std::transform(
                    extension.begin(),
                    extension.end(),
                    extension.begin(),
                    [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    }
                );

                if (excludedExtensions.count(extension)) {
                    continue;
                }
            }
        }

        filtered.push_back(url);
    }

    return filtered;
}

std::vector<std::string> UrlProcessor::process(
    const std::vector<std::string>& links,
    const std::string& baseUrl
) const {
    auto resolved = resolveRelative(links, baseUrl);
    auto fragmentFree = removeFragments(resolved);
    auto filtered = filterLinks(fragmentFree);
    return deduplicateLinks(filtered);
}

