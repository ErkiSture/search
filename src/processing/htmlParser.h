#pragma once

#include <lexbor/html/html.h>
#include <lexbor/dom/interfaces/element.h>

#include<vector>
#include<string>

struct ParsedPage {
    std::string title;
    std::string description;
    std::string text;
    std::vector<std::string> links;
};

class HtmlParser {
public:
    ParsedPage parse(const std::string& html) const;

private:
    std::string extractTitle(lxb_html_document_t* document) const;
    std::string extractDescription(lxb_html_document_t* document) const;
    std::string extractText(lxb_html_document_t* document) const;
    std::vector<std::string> extractLinks(lxb_html_document_t* document) const;
};