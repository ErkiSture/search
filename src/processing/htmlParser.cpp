
#include "htmlParser.h"

#include <lexbor/html/html.h>
#include <lexbor/dom/interfaces/element.h>
#include <lexbor/dom/interfaces/node.h>
#include <lexbor/dom/interfaces/text.h>
#include <lexbor/dom/interfaces/character_data.h>

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

namespace
{
    bool shouldSkipElement(lxb_dom_element_t* element)
    {
        const lxb_tag_id_t tag = lxb_dom_element_tag_id(element);

        return tag == LXB_TAG_SCRIPT
            || tag == LXB_TAG_STYLE
            || tag == LXB_TAG_NOSCRIPT
            || tag == LXB_TAG_TEMPLATE
            || tag == LXB_TAG_SVG
            || tag == LXB_TAG_CANVAS;
    }

    bool isBlockElement(lxb_dom_element_t* element)
    {
        const lxb_tag_id_t tag = lxb_dom_element_tag_id(element);

        switch (tag)
        {
        case LXB_TAG_ADDRESS:
        case LXB_TAG_ARTICLE:
        case LXB_TAG_ASIDE:
        case LXB_TAG_BLOCKQUOTE:
        case LXB_TAG_BR:
        case LXB_TAG_DD:
        case LXB_TAG_DIV:
        case LXB_TAG_DL:
        case LXB_TAG_DT:
        case LXB_TAG_FIELDSET:
        case LXB_TAG_FIGCAPTION:
        case LXB_TAG_FIGURE:
        case LXB_TAG_FOOTER:
        case LXB_TAG_FORM:
        case LXB_TAG_H1:
        case LXB_TAG_H2:
        case LXB_TAG_H3:
        case LXB_TAG_H4:
        case LXB_TAG_H5:
        case LXB_TAG_H6:
        case LXB_TAG_HEADER:
        case LXB_TAG_HR:
        case LXB_TAG_LI:
        case LXB_TAG_MAIN:
        case LXB_TAG_NAV:
        case LXB_TAG_OL:
        case LXB_TAG_P:
        case LXB_TAG_PRE:
        case LXB_TAG_SECTION:
        case LXB_TAG_TABLE:
        case LXB_TAG_TD:
        case LXB_TAG_TH:
        case LXB_TAG_TR:
        case LXB_TAG_UL:
            return true;

        default:
            return false;
        }
    }

    void appendSeparator(std::string& output)
    {
        if (!output.empty() &&
            !std::isspace(static_cast<unsigned char>(output.back())))
        {
            output.push_back(' ');
        }
    }

    void extractNodeText(lxb_dom_node_t* node, std::string& output)
    {
        if (node == nullptr)
        {
            return;
        }

        if (node->type == LXB_DOM_NODE_TYPE_ELEMENT)
        {
            lxb_dom_element_t* element =
                lxb_dom_interface_element(node);

            if (shouldSkipElement(element))
            {
                return;
            }

            if (isBlockElement(element))
            {
                appendSeparator(output);
            }
        }

        if (node->type == LXB_DOM_NODE_TYPE_TEXT)
        {
            lxb_dom_character_data_t* characterData =
                lxb_dom_interface_character_data(node);

            const lexbor_str_t& data = characterData->data;

            if (data.data != nullptr && data.length > 0)
            {
                output.append(
                    reinterpret_cast<const char*>(data.data),
                    data.length
                );
                output.push_back(' ');
            }

            return;
        }

        for (lxb_dom_node_t* child = node->first_child;
            child != nullptr;
            child = child->next)
        {
            extractNodeText(child, output);
        }

        if (node->type == LXB_DOM_NODE_TYPE_ELEMENT)
        {
            lxb_dom_element_t* element =
                lxb_dom_interface_element(node);

            if (isBlockElement(element))
            {
                appendSeparator(output);
            }
        }
    }

    std::string normalizeWhitespace(const std::string& input)
    {
        std::string output;
        output.reserve(input.size());

        bool previousWasWhitespace = true;

        for (unsigned char ch : input)
        {
            if (std::isspace(ch))
            {
                if (!previousWasWhitespace)
                {
                    output.push_back(' ');
                    previousWasWhitespace = true;
                }
            }
            else
            {
                output.push_back(static_cast<char>(ch));
                previousWasWhitespace = false;
            }
        }

        if (!output.empty() && output.back() == ' ')
        {
            output.pop_back();
        }

        return output;
    }
}

ParsedPage HtmlParser::parse(const std::string& html) const
{
    ParsedPage parsedPage;

    lxb_html_document_t* document = lxb_html_document_create();

    if (document == nullptr)
    {
        std::cerr << "Failed to create HTML document\n";
        return parsedPage;
    }

    const lxb_status_t status = lxb_html_document_parse(
        document,
        reinterpret_cast<const lxb_char_t*>(html.data()),
        html.size()
    );

    if (status != LXB_STATUS_OK)
    {
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
    lxb_html_document_t* document) const
{
    size_t length = 0;

    const lxb_char_t* title =
        lxb_html_document_title(document, &length);

    if (title == nullptr)
    {
        return {};
    }

    return std::string(
        reinterpret_cast<const char*>(title),
        length
    );
}

std::string HtmlParser::extractText(
    lxb_html_document_t* document) const
{
    if (document->body == nullptr)
    {
        return {};
    }

    std::string text;

    extractNodeText(
        lxb_dom_interface_node(document->body),
        text
    );

    return normalizeWhitespace(text);
}

std::vector<std::string> HtmlParser::extractLinks(
    lxb_html_document_t* document) const
{
    std::vector<std::string> links;

    if (document->body == nullptr)
    {
        return links;
    }

    lxb_dom_collection_t* collection =
        lxb_dom_collection_make(&document->dom_document, 128);

    if (collection == nullptr)
    {
        return links;
    }

    const lxb_status_t status = lxb_dom_elements_by_tag_name(
        lxb_dom_interface_element(document->body),
        collection,
        reinterpret_cast<const lxb_char_t*>("a"),
        1
    );

    if (status == LXB_STATUS_OK)
    {
        const size_t count = lxb_dom_collection_length(collection);

        for (size_t i = 0; i < count; ++i)
        {
            lxb_dom_element_t* element =
                lxb_dom_collection_element(collection, i);

            size_t length = 0;

            const lxb_char_t* href =
                lxb_dom_element_get_attribute(
                    element,
                    reinterpret_cast<const lxb_char_t*>("href"),
                    4,
                    &length
                );

            if (href != nullptr && length > 0)
            {
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
