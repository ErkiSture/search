
#include <lexbor/html/html.h>
#include <lexbor/dom/interfaces/element.h>

#include "htmlParser.h"

#include <iostream>
#include <string>
#include <vector>

ParsedPage HtmlParser::parse(const std::string& html) const {
    ParsedPage parsedPage;

    lxb_html_document_t* document = lxb_html_document_create();

    if (document == nullptr) {
        std::cerr << "Failed to create HTML document\n";
        return parsedPage;
    }

    const lxb_status_t status = lxb_html_document_parse(
        document,
        reinterpret_cast<const lxb_char_t*>(html.data()),
        html.size()
    );

    if (status != LXB_STATUS_OK) {
        std::cerr << "Failed to parse HTML\n";
        lxb_html_document_destroy(document);
        return parsedPage;
    }

    parsedPage.title = extractTitle(document);
    parsedPage.text = extractText(document);
    parsedPage.links = extractLinks(document);

    lxb_html_document_destroy(document);

    return parsedPage;
}

std::string HtmlParser::extractTitle(
    lxb_html_document_t* document
) const {
    size_t length = 0;

    const lxb_char_t* title =
        lxb_html_document_title(document, &length);

    if (title == nullptr) {
        return {};
    }

    return std::string(
        reinterpret_cast<const char*>(title),
        length
    );
}

std::string HtmlParser::extractText(
    lxb_html_document_t* document
) const {
    if (document->body == nullptr) {
        return {};
    }

    size_t length = 0;

    const lxb_char_t* text = lxb_dom_node_text_content(
        lxb_dom_interface_node(document->body),
        &length
    );

    if (text == nullptr) {
        return {};
    }

    return std::string(
        reinterpret_cast<const char*>(text),
        length
    );
}

std::vector<std::string> HtmlParser::extractLinks(
    lxb_html_document_t* document
) const {
    std::vector<std::string> links;

    if (document->body == nullptr) {
        return links;
    }

    lxb_dom_collection_t* collection =
        lxb_dom_collection_make(&document->dom_document, 128);

    if (collection == nullptr) {
        return links;
    }

    const lxb_status_t status = lxb_dom_elements_by_tag_name(
        lxb_dom_interface_element(document->body),
        collection,
        reinterpret_cast<const lxb_char_t*>("a"),
        1
    );

    if (status == LXB_STATUS_OK) {
        const size_t count = lxb_dom_collection_length(collection);

        for (size_t i = 0; i < count; ++i) {
            lxb_dom_element_t* element =
                lxb_dom_collection_element(collection, i);

            size_t length = 0;

            const lxb_char_t* href = lxb_dom_element_get_attribute(
                element,
                reinterpret_cast<const lxb_char_t*>("href"),
                4,
                &length
            );

            if (href != nullptr) {
                links.emplace_back(
                    reinterpret_cast<const char*>(href),
                    length
                );
            }
        }
    }

    lxb_dom_collection_destroy(collection, true);

    return links;
}
