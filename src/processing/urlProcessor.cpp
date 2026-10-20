#include "urlProcessor.h"
#include <curl/curl.h>
#include <unordered_set>
#include <vector>
#include <lexbor/url/url.h>

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

std::vector<std::string> UrlProcessor::process(
    const std::vector<std::string>& links,
    const std::string& baseUrl
) const {
    auto resolved = resolveRelative(links, baseUrl);
    auto fragmentFree = removeFragments(resolved);
    return deduplicateLinks(fragmentFree);
}

