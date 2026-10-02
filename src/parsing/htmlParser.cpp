#include"parsing/htmlParser.h"
#include<unordered_set>
#include<iostream>

void append_space(std::string& text) {
    if (!text.empty() && text.back() != ' ') {
        text += ' ';
    }
}

std::string strip_html(const std::string& html) {
    std::string text;
    bool inside_tag = false;
    bool inside_script = false;

    for (size_t i = 0; i < html.size(); i++) {
        if (!inside_tag && html.compare(i, 7, "<script") == 0) {
            inside_script = true;
        }
        if (!inside_tag && html.compare(i, 6, "<style") == 0) {
            inside_script = true;
        }
        if (inside_script && html.compare(i, 9, "</script>") == 0) {
            inside_script = false;
        }
        if (inside_script && html.compare(i, 8, "</style>") == 0) {
            inside_script = false;
        }

        char c = html[i];

        if (c == '<') {
            inside_tag = true;
            continue;
        }
        if (c == '>') {
            inside_tag = false;
            if(!inside_script) append_space(text);
            continue;
        }

        if (!inside_tag && !inside_script) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (uc >= 0x80 || std::isalnum(uc)) {
                text += c;
            }
            else {
                append_space(text);
            }
        }
    }

    // trim leading/trailing space
    size_t start = text.find_first_not_of(' ');
    size_t end = text.find_last_not_of(' ');
    return (start == std::string::npos) ? "" : text.substr(start, end - start + 1);
}