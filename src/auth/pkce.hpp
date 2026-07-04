#pragma once

#include <string>

struct PkcePair {
    std::string verifier;
    std::string challenge;
};

PkcePair make_pkce_pair();
std::string make_state_token();
