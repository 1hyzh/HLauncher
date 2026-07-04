#include "ui/launcher_window.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

std::string get_env_or_default(const char* name, const std::string& fallback) {
    if (const char* value = std::getenv(name)) {
        if (*value != '\0') {
            return value;
        }
    }
    return fallback;
}

} // namespace

int main(int argc, char** argv) {
    LauncherOptions options;
    options.tenant_id = get_env_or_default("HLAUNCHER_TENANT_ID", "common");
    options.redirect_uri = get_env_or_default("HLAUNCHER_REDIRECT_URI", "hl://auth");
    options.scope = get_env_or_default("HLAUNCHER_SCOPE", "openid offline_access XboxLive.signin");

    LauncherWindow window(options);
    return window.run(argc, argv);
}
