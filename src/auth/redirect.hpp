#pragma once

#include <string>

struct RedirectResult {
    std::string raw_url;
    std::string code;
    std::string state;
    std::string error;
    std::string error_description;
};

RedirectResult parse_redirect_url(const std::string& url);
