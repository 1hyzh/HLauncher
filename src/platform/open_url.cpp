#include "platform/open_url.hpp"

#if defined(_WIN32)
    #include <cstdint>
    #include <windows.h>
    #include <shellapi.h>
#elif defined(__APPLE__)
    #include <cstdlib>
#else
    #include <cstdlib>
#endif

bool open_url_in_browser(const std::string& url) {
#if defined(_WIN32)
    int size = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        return false;
    }

    std::wstring wide(size, L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wide.data(), size) <= 0) {
        return false;
    }

    HINSTANCE result = ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(result) > 32;
#elif defined(__APPLE__)
    std::string command = "open '" + url + "' >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
#else
    std::string command = "xdg-open '" + url + "' >/dev/null 2>&1";
    return std::system(command.c_str()) == 0;
#endif
}
