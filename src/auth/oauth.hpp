#pragma once

#include <string>

struct OAuthConfig {
    std::string tenant_id = "common";
    std::string client_id = "761f9fae-4ae5-4561-8d9d-2c20759ca82b";
    std::string redirect_uri = "hl://auth";
    std::string scope = "openid offline_access XboxLive.signin";
    std::string state;
    std::string code_challenge;
    std::string code_challenge_method = "S256";
};

std::string build_authorize_url(const OAuthConfig& config);
