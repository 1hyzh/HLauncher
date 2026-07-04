#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

class SingleInstance {
public:
    using RedirectHandler = std::function<void(const std::string&)>;

    explicit SingleInstance(std::string app_name);
    ~SingleInstance();

    bool acquire_primary();
    void start_server(RedirectHandler handler);
    bool forward_to_primary(const std::string& message) const;

private:
    void stop_server();

    std::string app_name_;
    std::uint16_t port_;
    bool is_primary_ = false;
    RedirectHandler handler_;
    std::thread server_thread_;
    std::atomic<bool> stop_requested_ = false;
    std::intptr_t listen_socket_ = -1;
};
