#pragma once

#include "app/account_store.hpp"
#include "app/instance_manager.hpp"
#include "auth/oauth.hpp"
#include "auth/redirect.hpp"
#include "ipc/single_instance.hpp"

#include <condition_variable>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include "app/modrinth.hpp"

struct LauncherOptions {
    std::string client_id;
    std::string tenant_id = "common";
    std::string redirect_uri = "hl://auth";
    std::string scope = "openid offline_access XboxLive.signin";
    std::string app_name = "HLauncher";
};

struct LauncherSettings {
    std::string java_path;
    int allocated_memory_mb = 4096;
    std::string jvm_args = "";
    bool force_mock = false;
};

class Launcher {
public:
    explicit Launcher(LauncherOptions options);
    ~Launcher();

    bool initialize_runtime(int argc, char** argv);
    LauncherSettings settings() const;
    void save_settings(const LauncherSettings& settings);
    bool start_auth_flow();
    bool handle_redirect_url(const std::string& url);
    void reload_accounts();
    void reload_instances();
    void add_offline_account(const std::string& username);
    void set_active_account(std::size_t index);
    void remove_active_account();
    bool auth_completed() const;
    bool auth_succeeded() const;
    std::string status_message() const;
    RedirectResult redirect_result() const;
    std::vector<AccountRecord> accounts() const;
    std::vector<InstanceRecord> instances() const;
    std::vector<VersionEntry> versions() const;
    InstanceRecord create_instance(const std::string& name, const std::string& version_id, const std::string& loader);
    LaunchPlan build_launch_plan(std::size_t index) const;
    void update_version_manifest_async();
    std::vector<std::pair<std::string, std::string>> detect_system_javas() const;
    bool install_modrinth_addons_async(const std::vector<ModrinthVersion>& versions, const std::string& type, const std::string& instance_name);
    bool download_portable_java_async();

    bool install_instance_async(const std::string& name);
    bool launch_instance_async(const std::string& name, bool force_mock);
    void kill_game_process();
    bool delete_instance(const std::string& name);

    bool is_installing() const;
    float install_progress() const;
    std::string install_stage() const;

    bool is_game_running() const;
    std::vector<std::string> game_logs() const;
    void clear_game_logs();
    bool is_instance_installed(const std::string& name) const;

private:
    void handle_redirect(const std::string& url);
    void wait_for_redirect();
    bool has_custom_uri_argument(int argc, char** argv, std::string& uri) const;

    LauncherOptions options_;
    SingleInstance instance_;
    OAuthConfig oauth_config_;
    AccountStore account_store_;
    InstanceManager instance_manager_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool redirect_received_ = false;
    bool redirect_valid_ = false;
    bool listener_started_ = false;
    RedirectResult redirect_result_;
    std::string code_verifier_;
    std::string code_challenge_;
    std::string state_;
    std::string status_message_ = "Ready to sign in.";
    std::vector<AccountRecord> accounts_;
    std::vector<InstanceRecord> instances_;
    std::vector<VersionEntry> versions_;

    // Installation state
    bool is_installing_ = false;
    float install_progress_ = 0.0f;
    std::string install_stage_;
    std::string installing_instance_;

    // Launch/Game process state
    bool is_game_running_ = false;
    std::vector<std::string> game_logs_;
    mutable std::mutex game_mutex_;
#if defined(_WIN32)
    void* game_process_handle_ = nullptr;
#else
    int game_pid_ = -1;
#endif

    LauncherSettings settings_;
    std::thread install_thread_;
    std::thread launch_thread_;
    std::thread manifest_thread_;
};
