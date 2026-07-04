#include "auth/redirect.hpp"

#include <cctype>
#include <string>
#include <string_view>
#include <unordered_map>

namespace {

std::string percent_decode(std::string_view value) {
    std::string decoded;
    decoded.reserve(value.size());

    auto hex_value = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
        if (ch >= 'A' && ch <= 'F') return 10 + (ch - 'A');
        return -1;
    };

    for (std::size_t i = 0; i < value.size(); ++i) {
        const char ch = value[i];
        if (ch == '%' && i + 2 < value.size()) {
            const int high = hex_value(value[i + 1]);
            const int low = hex_value(value[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded.push_back(static_cast<char>((high << 4) | low));
                i += 2;
            } else {
                decoded.push_back(ch);
            }
        } else if (ch == '+') {
            decoded.push_back(' ');
        } else {
            decoded.push_back(ch);
        }
    }

    return decoded;
}

std::unordered_map<std::string, std::string> parse_query(std::string_view query) {
    std::unordered_map<std::string, std::string> params;
    std::size_t start = 0;
    while (start < query.size()) {
        const std::size_t end = query.find('&', start);
        const std::size_t length = (end == std::string_view::npos) ? query.size() - start : end - start;
        const std::string_view item = query.substr(start, length);
        const std::size_t equal = item.find('=');
        if (equal == std::string_view::npos) {
            params.emplace(std::string(item), std::string());
        } else {
            const std::string key = percent_decode(item.substr(0, equal));
            const std::string value = percent_decode(item.substr(equal + 1));
            params.emplace(std::move(key), std::move(value));
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return params;
}

} // namespace

RedirectResult parse_redirect_url(const std::string& url) {
    RedirectResult result;
    result.raw_url = url;

    const std::size_t question = url.find('?');
    if (question == std::string::npos) {
        return result;
    }

    const auto params = parse_query(std::string_view(url).substr(question + 1));
    if (const auto it = params.find("code"); it != params.end()) {
        result.code = it->second;
    }
    if (const auto it = params.find("state"); it != params.end()) {
        result.state = it->second;
    }
    if (const auto it = params.find("error"); it != params.end()) {
        result.error = it->second;
    }
    if (const auto it = params.find("error_description"); it != params.end()) {
        result.error_description = it->second;
    }

    return result;
}
