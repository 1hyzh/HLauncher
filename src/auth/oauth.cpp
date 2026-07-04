#include "auth/oauth.hpp"

#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

bool is_unreserved(unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~';
}

std::string url_encode(const std::string& value) {
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;
    for (unsigned char ch : value) {
        if (is_unreserved(ch)) {
            encoded << static_cast<char>(ch);
        } else {
            encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return encoded.str();
}

} // namespace

std::string build_authorize_url(const OAuthConfig& config) {
    if (config.client_id.empty()) {
        throw std::invalid_argument("OAuth client_id must not be empty");
    }
    if (config.state.empty()) {
        throw std::invalid_argument("OAuth state must not be empty");
    }
    if (config.code_challenge.empty()) {
        throw std::invalid_argument("OAuth code_challenge must not be empty");
    }

    std::ostringstream url;
    url << "https://login.microsoftonline.com/" << url_encode(config.tenant_id)
        << "/oauth2/v2.0/authorize"
        << "?client_id=" << url_encode(config.client_id)
        << "&response_type=code"
        << "&redirect_uri=" << url_encode(config.redirect_uri)
        << "&response_mode=query"
        << "&scope=" << url_encode(config.scope)
        << "&state=" << url_encode(config.state)
        << "&code_challenge=" << url_encode(config.code_challenge)
        << "&code_challenge_method=" << url_encode(config.code_challenge_method)
        << "&prompt=select_account";

    return url.str();
}
