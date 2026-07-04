#include "ipc/single_instance.hpp"

#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(_WIN32)
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "Ws2_32.lib")
    using socket_type = SOCKET;
    static constexpr socket_type invalid_socket_value = INVALID_SOCKET;
#else
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <sys/socket.h>
    #include <unistd.h>
    using socket_type = int;
    static constexpr socket_type invalid_socket_value = -1;
#endif

namespace {

std::uint16_t port_for_app(const std::string& app_name) {
    std::uint32_t hash = 2166136261u;
    for (unsigned char ch : app_name) {
        hash ^= ch;
        hash *= 16777619u;
    }
    return static_cast<std::uint16_t>(30000u + (hash % 10000u));
}

#if defined(_WIN32)
struct WinsockRuntime {
    WinsockRuntime() {
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
    }

    ~WinsockRuntime() {
        WSACleanup();
    }
};

WinsockRuntime& ensure_winsock() {
    static WinsockRuntime runtime;
    return runtime;
}
#endif

socket_type create_server_socket(std::uint16_t port) {
#if defined(_WIN32)
    (void)ensure_winsock();
#endif

    socket_type sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == invalid_socket_value) {
        return invalid_socket_value;
    }

    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return invalid_socket_value;
    }

    if (listen(sock, 4) != 0) {
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return invalid_socket_value;
    }

    return sock;
}

bool connect_and_send(std::uint16_t port, const std::string& message) {
#if defined(_WIN32)
    (void)ensure_winsock();
#endif

    socket_type sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock == invalid_socket_value) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return false;
    }

    const char* data = message.data();
    std::size_t remaining = message.size();
    while (remaining > 0) {
#if defined(_WIN32)
        const int written = send(sock, data, static_cast<int>(remaining), 0);
#else
        const ssize_t written = send(sock, data, remaining, 0);
#endif
        if (written <= 0) {
#if defined(_WIN32)
            closesocket(sock);
#else
            close(sock);
#endif
            return false;
        }
        data += written;
        remaining -= static_cast<std::size_t>(written);
    }

#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif
    return true;
}

void close_socket(socket_type sock) {
    if (sock == invalid_socket_value) {
        return;
    }
#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif
}

} // namespace

SingleInstance::SingleInstance(std::string app_name)
    : app_name_(std::move(app_name)),
      port_(port_for_app(app_name_)) {}

SingleInstance::~SingleInstance() {
    stop_server();
}

bool SingleInstance::acquire_primary() {
    if (is_primary_) {
        return true;
    }

    const socket_type sock = create_server_socket(port_);
    if (sock == invalid_socket_value) {
        return false;
    }

    is_primary_ = true;
    listen_socket_ = static_cast<std::intptr_t>(sock);
    return true;
}

void SingleInstance::start_server(RedirectHandler handler) {
    if (!is_primary_ || listen_socket_ < 0) {
        return;
    }

    handler_ = std::move(handler);
    stop_requested_ = false;
    const socket_type server_socket = static_cast<socket_type>(listen_socket_);

    server_thread_ = std::thread([this, server_socket] {
        while (!stop_requested_) {
            sockaddr_in client_addr{};
#if defined(_WIN32)
            int client_size = sizeof(client_addr);
#else
            socklen_t client_size = sizeof(client_addr);
#endif
            const socket_type client = accept(server_socket, reinterpret_cast<sockaddr*>(&client_addr), &client_size);
            if (client == invalid_socket_value) {
                if (stop_requested_) {
                    break;
                }
                continue;
            }

            char buffer[4096];
#if defined(_WIN32)
            const int received = recv(client, buffer, static_cast<int>(sizeof(buffer) - 1), 0);
#else
            const ssize_t received = recv(client, buffer, sizeof(buffer) - 1, 0);
#endif
            if (received > 0) {
                buffer[received] = '\0';
                if (handler_) {
                    handler_(std::string(buffer, buffer + received));
                }
            }

            close_socket(client);
        }
    });
}

bool SingleInstance::forward_to_primary(const std::string& message) const {
    return connect_and_send(port_, message);
}

void SingleInstance::stop_server() {
    stop_requested_ = true;

    if (listen_socket_ >= 0) {
        const socket_type sock = static_cast<socket_type>(listen_socket_);
        close_socket(sock);
        listen_socket_ = -1;
    }

    if (server_thread_.joinable()) {
        server_thread_.join();
    }
}
