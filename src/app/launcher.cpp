#include "app/launcher.hpp"
#include "app/downloader.hpp"
#include "app/modrinth.hpp"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "auth/pkce.hpp"
#include "platform/open_url.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <utility>

#if defined(_WIN32)
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#endif

namespace {

bool starts_with_scheme(const std::string& value) {
    return value.rfind("hl://", 0) == 0;
}

std::filesystem::path make_accounts_storage_path(const std::string& app_name) {
#if defined(_WIN32)
    if (const char* appdata = std::getenv("APPDATA")) {
        return std::filesystem::path(appdata) / app_name / "accounts.txt";
    }
    return std::filesystem::current_path() / app_name / "accounts.txt";
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / "Library" / "Application Support" / app_name / "accounts.txt";
    }
    return std::filesystem::current_path() / app_name / "accounts.txt";
#else
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".local" / "share" / app_name / "accounts.txt";
    }
    return std::filesystem::current_path() / app_name / "accounts.txt";
#endif
}

std::string current_timestamp_utc() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t time_value = clock::to_time_t(now);
#if defined(_WIN32)
    std::tm tm_value{};
    gmtime_s(&tm_value, &time_value);
#else
    std::tm tm_value{};
    gmtime_r(&time_value, &tm_value);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M UTC", &tm_value);
    return buffer;
}

} // namespace

Launcher::Launcher(LauncherOptions options)
    : options_(std::move(options)),
      instance_(options_.app_name),
    account_store_(make_accounts_storage_path(options_.app_name)),
    instance_manager_(make_accounts_storage_path(options_.app_name).parent_path()) {
    oauth_config_.tenant_id = options_.tenant_id;
    oauth_config_.redirect_uri = options_.redirect_uri;
    oauth_config_.scope = options_.scope;

    if (!options_.client_id.empty()) {
        oauth_config_.client_id = options_.client_id;
    }

    accounts_ = account_store_.load();
    instances_ = instance_manager_.load_instances();

    auto app_dir = make_accounts_storage_path(options_.app_name).parent_path();
    auto cached_versions_path = app_dir / "versions.txt";
    VersionCatalog catalog;
    if (std::filesystem::exists(cached_versions_path)) {
        versions_ = catalog.load_from_file(cached_versions_path);
    }
    if (versions_.empty()) {
        versions_ = catalog.load_default_versions();
    }

    // Load Settings
    auto settings_path = app_dir / "settings.json";
    if (std::filesystem::exists(settings_path)) {
        std::ifstream s_in(settings_path);
        if (s_in.is_open()) {
            try {
                json s_json = json::parse(s_in);
                settings_.java_path = s_json.value("java_path", "");
                settings_.allocated_memory_mb = s_json.value("allocated_memory_mb", 4096);
                settings_.jvm_args = s_json.value("jvm_args", "");
                settings_.force_mock = s_json.value("force_mock", false);
            } catch (...) {}
            s_in.close();
        }
    }

    update_version_manifest_async();
}

Launcher::~Launcher() {
    kill_game_process();
    if (install_thread_.joinable()) install_thread_.join();
    if (launch_thread_.joinable()) launch_thread_.join();
    if (manifest_thread_.joinable()) manifest_thread_.join();
}

LauncherSettings Launcher::settings() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_;
}

void Launcher::save_settings(const LauncherSettings& settings) {
    std::lock_guard<std::mutex> lock(mutex_);
    settings_ = settings;
    
    auto app_dir = make_accounts_storage_path(options_.app_name).parent_path();
    std::filesystem::create_directories(app_dir);
    auto settings_path = app_dir / "settings.json";
    std::ofstream s_out(settings_path, std::ios::trunc);
    if (s_out.is_open()) {
        try {
            json s_json;
            s_json["java_path"] = settings_.java_path;
            s_json["allocated_memory_mb"] = settings_.allocated_memory_mb;
            s_json["jvm_args"] = settings_.jvm_args;
            s_json["force_mock"] = settings_.force_mock;
            s_out << s_json.dump(4);
        } catch (...) {}
        s_out.close();
    }
}

bool Launcher::has_custom_uri_argument(int argc, char** argv, std::string& uri) const {
    if (argc < 2 || argv == nullptr || argv[1] == nullptr) {
        return false;
    }

    uri = argv[1];
    return starts_with_scheme(uri);
}

bool Launcher::initialize_runtime(int argc, char** argv) {
    if (options_.tenant_id != "common") {
        std::cerr << "Warning: HLAUNCHER_TENANT_ID is not 'common'. Personal Microsoft accounts may fail.\n";
    }

    std::string incoming_uri;
    const bool has_uri = has_custom_uri_argument(argc, argv, incoming_uri);

    if (has_uri && !instance_.acquire_primary()) {
        if (instance_.forward_to_primary(incoming_uri)) {
            return false;
        }
        std::cerr << "Unable to forward redirect URI to the running instance.\n";
        return false;
    }

    if (!instance_.acquire_primary()) {
        std::cerr << "Another launcher instance is already active.\n";
        return false;
    }

    if (!listener_started_) {
        instance_.start_server([this](const std::string& url) {
            handle_redirect(url);
        });
        listener_started_ = true;
    }

    if (has_uri) {
        handle_redirect(incoming_uri);
    }

    return true;
}

bool Launcher::start_auth_flow() {
    if (!listener_started_) {
        instance_.start_server([this](const std::string& url) {
            handle_redirect(url);
        });
        listener_started_ = true;
    }

    const PkcePair pkce = make_pkce_pair();
    const std::string state = make_state_token();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        code_verifier_ = pkce.verifier;
        code_challenge_ = pkce.challenge;
        state_ = state;
        redirect_received_ = false;
        redirect_valid_ = false;
        redirect_result_ = {};
        status_message_ = "Opening browser for Microsoft sign-in...";
    }

    oauth_config_.state = state;
    oauth_config_.code_challenge = pkce.challenge;

    const std::string auth_url = build_authorize_url(oauth_config_);
    std::cout << auth_url << '\n';

    if (!open_url_in_browser(auth_url)) {
        std::lock_guard<std::mutex> lock(mutex_);
        status_message_ = "Failed to open the browser automatically. Open the URL above manually.";
        return false;
    }

    return true;
}

bool Launcher::handle_redirect_url(const std::string& url) {
    if (!starts_with_scheme(url)) {
        return false;
    }

    handle_redirect(url);
    return true;
}

void Launcher::handle_redirect(const std::string& url) {
    const RedirectResult result = parse_redirect_url(url);
    bool should_add_account = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        redirect_result_ = result;
        redirect_received_ = true;
        redirect_valid_ = result.error.empty() && !result.code.empty() && result.state == state_;
        if (redirect_valid_) {
            status_message_ = "Authorization code received successfully. Account saved.";
            should_add_account = true;
        } else if (!redirect_result_.error.empty()) {
            status_message_ = "OAuth error: " + redirect_result_.error;
        } else if (!result.state.empty() && result.state != state_) {
            status_message_ = "State mismatch. Rejecting the redirect response.";
        } else {
            status_message_ = "Authentication redirect did not contain a valid code.";
        }
    }

    if (should_add_account) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status_message_ = "Exchanging OAuth authorization code...";
        }
        
        std::thread([this, result]() {
            std::string display_name = "Microsoft account";
            try {
                std::string token_url = "https://login.microsoftonline.com/common/oauth2/v2.0/token";
                std::string post_data = "client_id=" + options_.client_id +
                                        "&scope=" + options_.scope +
                                        "&code=" + result.code +
                                        "&redirect_uri=" + options_.redirect_uri +
                                        "&grant_type=authorization_code" +
                                        "&code_verifier=" + code_verifier_;
                
                std::string curl_cmd = "curl -s -X POST -H \"Content-Type: application/x-www-form-urlencoded\" "
                                       "-d \"" + post_data + "\" \"" + token_url + "\"";
                
                FILE* fp = popen(curl_cmd.c_str(), "r");
                if (fp) {
                    std::string response;
                    char buffer[4096];
                    while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
                        response += buffer;
                    }
                    pclose(fp);
                    
                    if (!response.empty()) {
                        auto token_json = json::parse(response);
                        std::string access_token = token_json.value("access_token", "");
                                if (!access_token.empty()) {
                                    // Step 2: Authenticate with Xbox Live
                                    json xbl_body = {
                                        {"Properties", {
                                            {"AuthMethod", "RPS"},
                                            {"SiteName", "user.auth.xboxlive.com"},
                                            {"RpsTicket", "d=" + access_token}
                                        }},
                                        {"RelyingParty", "http://auth.xboxlive.com"},
                                        {"TokenType", "JWT"}
                                    };
                                    std::string xbl_post = xbl_body.dump();
                                    std::string xbl_post_escaped;
                                    for (char c : xbl_post) {
                                        if (c == '"') xbl_post_escaped += "\\\"";
                                        else xbl_post_escaped += c;
                                    }
                                    
                                    std::string xbl_cmd = "curl -s -X POST -H \"Content-Type: application/json\" -H \"Accept: application/json\" "
                                                          "-d \"" + xbl_post_escaped + "\" \"https://user.auth.xboxlive.com/user/authenticate\"";
                                    std::string xbl_res;
                                    FILE* fp_xbl = popen(xbl_cmd.c_str(), "r");
                                    if (fp_xbl) {
                                        while (fgets(buffer, sizeof(buffer), fp_xbl) != nullptr) {
                                            xbl_res += buffer;
                                        }
                                        pclose(fp_xbl);
                                    }
                                    
                                    auto xbl_json = json::parse(xbl_res);
                                    std::string xbl_token = xbl_json.value("Token", "");
                                    std::string uhs;
                                    if (xbl_json.contains("DisplayClaims") && xbl_json["DisplayClaims"].contains("xui") && !xbl_json["DisplayClaims"]["xui"].empty()) {
                                        uhs = xbl_json["DisplayClaims"]["xui"][0].value("uhs", "");
                                    }
                                    
                                    if (!xbl_token.empty() && !uhs.empty()) {
                                        // Step 3: Authenticate with XSTS
                                        json xsts_body = {
                                            {"Properties", {
                                                {"SandboxId", "RETAIL"},
                                                {"UserTokens", { xbl_token }}
                                            }},
                                            {"RelyingParty", "rp://api.minecraftservices.com/"},
                                            {"TokenType", "JWT"}
                                        };
                                        std::string xsts_post = xsts_body.dump();
                                        std::string xsts_post_escaped;
                                        for (char c : xsts_post) {
                                            if (c == '"') xsts_post_escaped += "\\\"";
                                            else xsts_post_escaped += c;
                                        }
                                        
                                        std::string xsts_cmd = "curl -s -X POST -H \"Content-Type: application/json\" -H \"Accept: application/json\" "
                                                               "-d \"" + xsts_post_escaped + "\" \"https://xsts.auth.xboxlive.com/xsts/authorize\"";
                                        std::string xsts_res;
                                        FILE* fp_xsts = popen(xsts_cmd.c_str(), "r");
                                        if (fp_xsts) {
                                            while (fgets(buffer, sizeof(buffer), fp_xsts) != nullptr) {
                                                xsts_res += buffer;
                                            }
                                            pclose(fp_xsts);
                                        }
                                        
                                        auto xsts_json = json::parse(xsts_res);
                                        std::string xsts_token = xsts_json.value("Token", "");
                                        
                                        if (!xsts_token.empty()) {
                                            // Step 4: Authenticate with Minecraft
                                            json mc_body = {
                                                {"identityToken", "XBL3.0 x=" + uhs + ";" + xsts_token}
                                            };
                                            std::string mc_post = mc_body.dump();
                                            std::string mc_post_escaped;
                                            for (char c : mc_post) {
                                                if (c == '"') mc_post_escaped += "\\\"";
                                                else mc_post_escaped += c;
                                            }
                                            
                                            std::string mc_cmd = "curl -s -X POST -H \"Content-Type: application/json\" "
                                                                 "-d \"" + mc_post_escaped + "\" \"https://api.minecraftservices.com/authentication/login_with_xbox\"";
                                            std::string mc_res;
                                            FILE* fp_mc = popen(mc_cmd.c_str(), "r");
                                            if (fp_mc) {
                                                while (fgets(buffer, sizeof(buffer), fp_mc) != nullptr) {
                                                    mc_res += buffer;
                                                }
                                                pclose(fp_mc);
                                            }
                                            
                                            auto mc_json = json::parse(mc_res);
                                            std::string mc_token = mc_json.value("access_token", "");
                                            
                                            if (!mc_token.empty()) {
                                                // Step 5: Fetch Minecraft Profile Name
                                                std::string profile_cmd = "curl -s -H \"Authorization: Bearer " + mc_token + "\" \"https://api.minecraftservices.com/minecraft/profile\"";
                                                std::string profile_res;
                                                FILE* fp_profile = popen(profile_cmd.c_str(), "r");
                                                if (fp_profile) {
                                                    while (fgets(buffer, sizeof(buffer), fp_profile) != nullptr) {
                                                        profile_res += buffer;
                                                    }
                                                    pclose(fp_profile);
                                                }
                                                
                                                auto profile_json = json::parse(profile_res);
                                                std::string mc_name = profile_json.value("name", "");
                                                if (!mc_name.empty()) {
                                                    display_name = mc_name;
                                                }
                                            }
                                        }
                                    }
                                }
                    }
                }
            } catch (...) {}
            
            std::vector<AccountRecord> snapshot;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                accounts_.erase(
                    std::remove_if(accounts_.begin(), accounts_.end(), [&](const auto& a) { return a.display_name == display_name; }),
                    accounts_.end()
                );
                accounts_.push_back(AccountRecord{
                    "Microsoft",
                    display_name,
                    "Signed in",
                    current_timestamp_utc()
                });
                std::rotate(accounts_.rbegin(), accounts_.rbegin() + 1, accounts_.rend());
                snapshot = accounts_;
                status_message_ = "Signed in successfully as: " + display_name;
            }
            try {
                account_store_.save(snapshot);
            } catch (...) {}
        }).detach();
    }

    cv_.notify_all();
}

void Launcher::wait_for_redirect() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] {
        return redirect_received_;
    });

    if (!redirect_valid_) {
        std::cerr << "Authentication redirect did not contain a valid code.\n";
        if (!redirect_result_.error.empty()) {
            std::cerr << "OAuth error: " << redirect_result_.error << '\n';
            if (!redirect_result_.error_description.empty()) {
                std::cerr << redirect_result_.error_description << '\n';
            }
        }
        return;
    }

    if (!state_.empty() && redirect_result_.state != state_) {
        std::cerr << "State mismatch. Rejecting the redirect response.\n";
        redirect_valid_ = false;
        return;
    }

    std::cout << "Authorization code received successfully.\n";
    std::cout << "Code: " << redirect_result_.code << '\n';
    std::cout << "State: " << redirect_result_.state << '\n';
    std::cout << "Next step: exchange the code for Microsoft, Xbox Live, XSTS, and Minecraft tokens.\n";
}

bool Launcher::auth_completed() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return redirect_received_;
}

bool Launcher::auth_succeeded() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return redirect_received_ && redirect_valid_;
}

std::string Launcher::status_message() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_message_;
}

RedirectResult Launcher::redirect_result() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return redirect_result_;
}

std::vector<AccountRecord> Launcher::accounts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return accounts_;
}

void Launcher::reload_accounts() {
    std::lock_guard<std::mutex> lock(mutex_);
    accounts_ = account_store_.load();
}

void Launcher::add_offline_account(const std::string& username) {
    if (username.empty()) return;
    std::vector<AccountRecord> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        accounts_.erase(
            std::remove_if(accounts_.begin(), accounts_.end(), [&](const auto& a) { return a.display_name == username; }),
            accounts_.end()
        );
        accounts_.push_back(AccountRecord{
            "offline",
            username,
            "Offline Mode",
            current_timestamp_utc()
        });
        std::rotate(accounts_.rbegin(), accounts_.rbegin() + 1, accounts_.rend());
        snapshot = accounts_;
    }
    try {
        account_store_.save(snapshot);
    } catch (...) {}
}

void Launcher::set_active_account(std::size_t index) {
    std::vector<AccountRecord> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= accounts_.size()) return;
        auto active = accounts_[index];
        accounts_.erase(accounts_.begin() + index);
        accounts_.insert(accounts_.begin(), active);
        snapshot = accounts_;
    }
    try {
        account_store_.save(snapshot);
    } catch (...) {}
}

void Launcher::remove_active_account() {
    std::vector<AccountRecord> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (accounts_.empty()) return;
        accounts_.erase(accounts_.begin());
        snapshot = accounts_;
    }
    try {
        account_store_.save(snapshot);
    } catch (...) {}
}

void Launcher::reload_instances() {
    std::lock_guard<std::mutex> lock(mutex_);
    instances_ = instance_manager_.load_instances();
    versions_ = instance_manager_.available_versions();
}

std::vector<InstanceRecord> Launcher::instances() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return instances_;
}

std::vector<VersionEntry> Launcher::versions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return versions_;
}

InstanceRecord Launcher::create_instance(const std::string& name, const std::string& version_id, const std::string& loader) {
    auto created = instance_manager_.create_instance(name, version_id, loader);
    std::lock_guard<std::mutex> lock(mutex_);
    instances_ = instance_manager_.load_instances();
    return created;
}

LaunchPlan Launcher::build_launch_plan(std::size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= instances_.size()) {
        return {};
    }
    auto plan = instance_manager_.build_launch_plan(instances_[index]);
    if (!accounts_.empty()) {
        const auto& active = accounts_.front();
        for (std::size_t i = 0; i < plan.arguments.size(); ++i) {
            if (plan.arguments[i] == "--username" && i + 1 < plan.arguments.size()) {
                plan.arguments[i + 1] = active.display_name;
            }
            if (plan.arguments[i] == "--uuid" && i + 1 < plan.arguments.size()) {
                std::hash<std::string> hasher;
                size_t h = hasher(active.display_name);
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%016zx%016zx", h, h ^ 0xDEADBEEFCAFEBABE);
                std::string hex(buf);
                std::string formatted_uuid = hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) + "-" + hex.substr(16, 4) + "-" + hex.substr(20, 12);
                plan.arguments[i + 1] = formatted_uuid;
            }
        }
    }
    return plan;
}

bool Launcher::install_instance_async(const std::string& name) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_installing_) {
            return false;
        }
        is_installing_ = true;
        install_progress_ = 0.0f;
        install_stage_ = "Connecting to Mojang update servers...";
        installing_instance_ = name;
    }

    if (install_thread_.joinable()) {
        install_thread_.join();
    }
    install_thread_ = std::thread([this, name]() {
        InstanceRecord instance;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& inst : instances_) {
                if (inst.name == name) {
                    instance = inst;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            std::lock_guard<std::mutex> lock(mutex_);
            is_installing_ = false;
            status_message_ = "Error: Instance not found.";
            return;
        }

        auto app_dir = make_accounts_storage_path(options_.app_name).parent_path();
        std::filesystem::create_directories(app_dir);

        auto progress_cb = [this](float pct, const std::string& stage) {
            std::lock_guard<std::mutex> lock(mutex_);
            install_progress_ = pct / 100.0f;
            install_stage_ = stage;
        };

        bool ok = downloader::install_minecraft(instance.directory, instance.version_id, instance.loader, app_dir, progress_cb);

        if (ok) {
            try {
                std::ofstream inst_file(instance.directory / "installed.txt");
                inst_file << "name=" << instance.name << "\n";
                inst_file << "version=" << instance.version_id << "\n";
                inst_file << "loader=" << instance.loader << "\n";
                inst_file << "installed_at=" << current_timestamp_utc() << "\n";
                inst_file.close();

                std::ofstream py_file(instance.directory / "launch_mock.py");
                py_file << R"PYTHON(import sys
import time
import tkinter as tk

print("[HLauncher Mock] Starting Minecraft simulation...")
print("[HLauncher Mock] Arguments:")
for i, arg in enumerate(sys.argv):
    print(f"  [{i}]: {arg}")

stages = [
    "[Client thread/INFO]: Setting user: Player",
    "[Client thread/INFO]: LWJGL Version: 3.3.3-build-12",
    "[Client thread/INFO]: Initializing OpenAL sound engine",
    "[Client thread/INFO]: Created sound buffer 42",
    "[Client thread/INFO]: Texture atlas created: 1024x1024",
    "[Client thread/INFO]: Loaded 412 resources",
    "[Client thread/INFO]: Game window initialized successfully."
]

for stage in stages:
    print(stage)
    sys.stdout.flush()
    time.sleep(0.3)

try:
    root = tk.Tk()
    root.title("HLauncher - Minecraft Mock Screen")
    root.geometry("800x500")
    root.configure(bg="#1a1c23")
    
    lbl_title = tk.Label(root, text="MINECRAFT (MOCK RUNTIME)", fg="#00ffcc", bg="#1a1c23", font=("Helvetica", 20, "bold"))
    lbl_title.pack(pady=20)
    
    lbl_status = tk.Label(root, text="This window simulates Minecraft running under HLauncher.", fg="#a0a5b5", bg="#1a1c23", font=("Helvetica", 12))
    lbl_status.pack(pady=5)
    
    frame = tk.Frame(root, bg="#232631", bd=2, relief=tk.GROOVE)
    frame.pack(padx=30, pady=20, fill=tk.BOTH, expand=True)
    
    txt = tk.Text(frame, bg="#232631", fg="#e2e8f0", font=("Courier", 11), bd=0, highlightthickness=0)
    txt.pack(padx=15, pady=15, fill=tk.BOTH, expand=True)
    
    txt.insert(tk.END, "PASSED ARGUMENTS:\n")
    for arg in sys.argv:
        txt.insert(tk.END, f"  {arg}\n")
    txt.insert(tk.END, "\nSTATUS:\n")
    txt.insert(tk.END, "  - Game Loop Started\n")
    txt.insert(tk.END, "  - GPU Renderer: MockOpenGL 4.5 Core\n")
    txt.insert(tk.END, "  - FPS: 60 (VSync Active)\n")
    txt.insert(tk.END, "\nClose this window to return to HLauncher.\n")
    txt.config(state=tk.DISABLED)
    
    root.mainloop()
except Exception as e:
    print(f"[Client thread/ERROR]: Failed to open Tkinter window: {e}")
    print("[Client thread/INFO]: Running in console-only mode for 10 seconds.")
    for i in range(10):
        print(f"[Client thread/INFO]: Mock game tick {i}...")
        sys.stdout.flush()
        time.sleep(1.0)

print("[Client thread/INFO]: Stopping!")
print("[Client thread/INFO]: Sound system shut down.")
print("[Client thread/INFO]: Releasing OpenGL Context.")
)PYTHON";
                py_file.close();

                std::lock_guard<std::mutex> lock(mutex_);
                is_installing_ = false;
                status_message_ = "Installation successful for " + name;
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(mutex_);
                is_installing_ = false;
                status_message_ = std::string("Post-install configuration error: ") + e.what();
            }
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            is_installing_ = false;
            status_message_ = "Installation failed for " + name;
        }
    });

    return true;
}

bool Launcher::launch_instance_async(const std::string& name, bool force_mock) {
    InstanceRecord instance;
    bool found = false;
    std::size_t inst_idx = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_game_running_) {
            return false;
        }
        for (std::size_t i = 0; i < instances_.size(); ++i) {
            if (instances_[i].name == name) {
                instance = instances_[i];
                found = true;
                inst_idx = i;
                break;
            }
        }
    }

    if (!found) {
        return false;
    }

    LaunchPlan plan = build_launch_plan(inst_idx);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        is_game_running_ = true;
        status_message_ = "Launching " + name + "...";
    }

    {
        std::lock_guard<std::mutex> lock(game_mutex_);
        game_logs_.clear();
        game_logs_.push_back("[Launcher] Preparing process launch...");
    }

    if (launch_thread_.joinable()) {
        launch_thread_.join();
    }

    launch_thread_ = std::thread([this, instance, plan, force_mock]() {
        // Read active settings
        LauncherSettings s;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            s = settings_;
        }

        bool use_java = !force_mock && !s.force_mock;
        std::string command;
        std::vector<std::string> args;
        bool has_launch_args = false;

        std::filesystem::path launch_args_path = instance.directory / "launch_args.txt";
        if (use_java && std::filesystem::exists(launch_args_path)) {
            std::ifstream file(launch_args_path);
            if (file.is_open()) {
                std::string j_bin, main_class, classpath, asset_index;
                if (std::getline(file, j_bin) &&
                    std::getline(file, main_class) &&
                    std::getline(file, classpath) &&
                    std::getline(file, asset_index)) {
                    
                    if (!s.java_path.empty()) {
                        command = s.java_path;
                    } else {
                        command = j_bin;
                    }
                    
                    args.push_back(command);

                    // Memory allocation
                    args.push_back("-Xmx" + std::to_string(s.allocated_memory_mb) + "m");

#if defined(__APPLE__)
                    args.push_back("-XstartOnFirstThread");
#endif

                    // JVM Arguments parsing
                    if (!s.jvm_args.empty()) {
                        std::stringstream ss(s.jvm_args);
                        std::string chunk;
                        while (ss >> chunk) {
                            if (!chunk.empty()) {
                                args.push_back(chunk);
                            }
                        }
                    }

                    args.push_back("-cp");
                    args.push_back(classpath);
                    
                    std::filesystem::path natives_dir = instance.directory / "natives";
                    if (std::filesystem::exists(natives_dir)) {
                        args.push_back("-Djava.library.path=" + natives_dir.string());
                    }
                    
                    args.push_back(main_class);
                    
                    std::string game_arg;
                    while (std::getline(file, game_arg)) {
                        if (!game_arg.empty() && game_arg != "--demo") {
                            args.push_back(game_arg);
                        }
                    }
                    
                    AccountRecord active_account;
                    bool has_active = false;
                    {
                        std::lock_guard<std::mutex> lock(mutex_);
                        if (!accounts_.empty()) {
                            active_account = accounts_.front();
                            has_active = true;
                        }
                    }
                    
                    if (has_active) {
                        for (std::size_t i = 0; i < args.size(); ++i) {
                            if (args[i] == "--username" && i + 1 < args.size()) {
                                args[i + 1] = active_account.display_name;
                            }
                            if (args[i] == "--uuid" && i + 1 < args.size()) {
                                std::hash<std::string> hasher;
                                size_t h = hasher(active_account.display_name);
                                char buf[64];
                                std::snprintf(buf, sizeof(buf), "%016zx%016zx", h, h ^ 0xDEADBEEFCAFEBABE);
                                std::string hex(buf);
                                std::string formatted_uuid = hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) + "-" + hex.substr(16, 4) + "-" + hex.substr(20, 12);
                                args[i + 1] = formatted_uuid;
                            }
                        }
                    }
                    
                    has_launch_args = true;
                }
            }
        }

        if (!has_launch_args) {
            if (use_java) {
#if defined(_WIN32)
                // Windows java check
#else
                int check = std::system("java -version >/dev/null 2>&1");
                if (check != 0 && s.java_path.empty()) {
                    use_java = false;
                    std::lock_guard<std::mutex> lock(game_mutex_);
                    game_logs_.push_back("[Launcher/WARNING] Java was not found in PATH. Falling back to Mock Game runtime.");
                }
#endif
            }

            if (use_java) {
                if (!s.java_path.empty()) {
                    command = s.java_path;
                } else {
                    command = plan.java_executable.string();
                }
                args.push_back(command);
                args.push_back("-Xmx" + std::to_string(s.allocated_memory_mb) + "m");
#if defined(__APPLE__)
                args.push_back("-XstartOnFirstThread");
#endif
                if (!s.jvm_args.empty()) {
                    std::stringstream ss(s.jvm_args);
                    std::string chunk;
                    while (ss >> chunk) {
                        if (!chunk.empty()) {
                            args.push_back(chunk);
                        }
                    }
                }
                args.push_back("-cp");
                args.push_back((instance.directory / "client.jar").string());
                args.push_back(plan.main_class);
                for (const auto& arg : plan.arguments) {
                    args.push_back(arg);
                }
            } else {
                command = "python3";
                args.push_back(command);
                args.push_back((instance.directory / "launch_mock.py").string());
                for (const auto& arg : plan.arguments) {
                    args.push_back(arg);
                }
            }
        }

        std::string cmd_str;
        for (const auto& arg : args) {
            cmd_str += "\"" + arg + "\" ";
        }
        {
            std::lock_guard<std::mutex> lock(game_mutex_);
            game_logs_.push_back("[Launcher] Running command: " + cmd_str);
        }

#if defined(_WIN32)
        // Set up the security attributes structure to allow pipe handles to be inherited.
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        HANDLE hChildStd_OUT_Rd = NULL;
        HANDLE hChildStd_OUT_Wr = NULL;

        // Create a pipe for the child process's STDOUT/STDERR.
        if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
            std::lock_guard<std::mutex> lock(game_mutex_);
            game_logs_.push_back("[Launcher/ERROR] Stdout pipe creation failed.");
            std::lock_guard<std::mutex> lock2(mutex_);
            is_game_running_ = false;
            status_message_ = "Launch failed.";
            return;
        }

        // Ensure the read handle to the pipe for STDOUT is not inherited.
        if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
            std::lock_guard<std::mutex> lock(game_mutex_);
            game_logs_.push_back("[Launcher/ERROR] Stdout pipe handle configuration failed.");
            CloseHandle(hChildStd_OUT_Rd);
            CloseHandle(hChildStd_OUT_Wr);
            std::lock_guard<std::mutex> lock2(mutex_);
            is_game_running_ = false;
            status_message_ = "Launch failed.";
            return;
        }

        // Build the command line string.
        std::string cmd_line;
        for (std::size_t i = 0; i < args.size(); ++i) {
            cmd_line += "\"" + args[i] + "\"";
            if (i + 1 < args.size()) {
                cmd_line += " ";
            }
        }

        // Make a mutable copy of the command line string (CreateProcessA requires it)
        std::vector<char> cmd_line_chars(cmd_line.begin(), cmd_line.end());
        cmd_line_chars.push_back('\0');

        PROCESS_INFORMATION piProcInfo;
        STARTUPINFOA siStartInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFOA));
        siStartInfo.cb = sizeof(STARTUPINFOA);
        siStartInfo.hStdError = hChildStd_OUT_Wr;
        siStartInfo.hStdOutput = hChildStd_OUT_Wr;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        // Create the child process.
        BOOL bSuccess = CreateProcessA(
            NULL, 
            cmd_line_chars.data(), 
            NULL,          
            NULL,          
            TRUE,          
            CREATE_NO_WINDOW, 
            NULL,          
            NULL,          
            &siStartInfo,  
            &piProcInfo    
        );

        // Close the write end of the pipe in the parent process so ReadFile knows when EOF is reached
        CloseHandle(hChildStd_OUT_Wr);

        if (!bSuccess) {
            std::lock_guard<std::mutex> lock(game_mutex_);
            game_logs_.push_back("[Launcher/ERROR] CreateProcessA failed to start the game process (Error: " + std::to_string(GetLastError()) + ").");
            CloseHandle(hChildStd_OUT_Rd);
            std::lock_guard<std::mutex> lock2(mutex_);
            is_game_running_ = false;
            status_message_ = "Launch failed.";
            return;
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            game_process_handle_ = piProcInfo.hProcess;
        }

        // Read output from the child process's pipe.
        char buffer[256];
        DWORD dwRead;
        std::string line_accumulator;
        while (ReadFile(hChildStd_OUT_Rd, buffer, sizeof(buffer) - 1, &dwRead, NULL) && dwRead > 0) {
            buffer[dwRead] = '\0';
            for (DWORD i = 0; i < dwRead; ++i) {
                if (buffer[i] == '\n') {
                    std::lock_guard<std::mutex> lock(game_mutex_);
                    game_logs_.push_back(line_accumulator);
                    line_accumulator.clear();
                } else if (buffer[i] != '\r') {
                    line_accumulator.push_back(buffer[i]);
                }
            }
        }
        if (!line_accumulator.empty()) {
            std::lock_guard<std::mutex> lock(game_mutex_);
            game_logs_.push_back(line_accumulator);
        }

        // Wait for the process to terminate.
        WaitForSingleObject(piProcInfo.hProcess, INFINITE);

        // Clean up handles.
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(piProcInfo.hThread);
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            CloseHandle(piProcInfo.hProcess);
            game_process_handle_ = nullptr;
        }
#else
        int pipefd[2];
        if (pipe(pipefd) == -1) {
            std::lock_guard<std::mutex> lock(game_mutex_);
            game_logs_.push_back("[Launcher/ERROR] Pipe creation failed.");
            std::lock_guard<std::mutex> lock2(mutex_);
            is_game_running_ = false;
            status_message_ = "Launch failed.";
            return;
        }

        pid_t pid = fork();
        if (pid == -1) {
            std::lock_guard<std::mutex> lock(game_mutex_);
            game_logs_.push_back("[Launcher/ERROR] Fork failed.");
            close(pipefd[0]);
            close(pipefd[1]);
            std::lock_guard<std::mutex> lock2(mutex_);
            is_game_running_ = false;
            status_message_ = "Launch failed.";
            return;
        }

        if (pid == 0) {
            dup2(pipefd[1], STDOUT_FILENO);
            dup2(pipefd[1], STDERR_FILENO);
            close(pipefd[0]);
            close(pipefd[1]);

            std::vector<char*> c_args;
            c_args.reserve(args.size() + 1);
            for (auto& arg : args) {
                c_args.push_back(arg.data());
            }
            c_args.push_back(nullptr);

            execvp(c_args[0], c_args.data());
            std::cerr << "[Child/ERROR] execvp failed to start " << c_args[0] << std::endl;
            exit(127);
        } else {
            close(pipefd[1]);
            {
                std::lock_guard<std::mutex> lock(mutex_);
                game_pid_ = pid;
            }

            char buffer[512];
            std::string line_accumulator;
            ssize_t bytes_read;
            while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[bytes_read] = '\0';
                for (ssize_t i = 0; i < bytes_read; ++i) {
                    if (buffer[i] == '\n') {
                        std::lock_guard<std::mutex> lock(game_mutex_);
                        game_logs_.push_back(line_accumulator);
                        line_accumulator.clear();
                    } else if (buffer[i] != '\r') {
                        line_accumulator.push_back(buffer[i]);
                    }
                }
            }
            if (!line_accumulator.empty()) {
                std::lock_guard<std::mutex> lock(game_mutex_);
                game_logs_.push_back(line_accumulator);
            }
            close(pipefd[0]);

            int status = 0;
            waitpid(pid, &status, 0);
        }
#endif

        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_game_running_ = false;
#if !defined(_WIN32)
            game_pid_ = -1;
#endif
            status_message_ = instance.name + " game process exited.";
        }
        {
            std::lock_guard<std::mutex> lock(game_mutex_);
            game_logs_.push_back("[Launcher] Game process terminated.");
        }
    });

    return true;
}

void Launcher::kill_game_process() {
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(mutex_);
    if (game_process_handle_ != nullptr) {
        TerminateProcess(static_cast<HANDLE>(game_process_handle_), 1);
    }
#else
    int pid_to_kill = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pid_to_kill = game_pid_;
    }
    if (pid_to_kill > 0) {
        kill(pid_to_kill, SIGTERM);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        kill(pid_to_kill, SIGKILL);
    }
#endif
}

bool Launcher::delete_instance(const std::string& name) {
    std::filesystem::path dir_to_remove;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& inst : instances_) {
            if (inst.name == name) {
                dir_to_remove = inst.directory;
                break;
            }
        }
    }
    if (!dir_to_remove.empty()) {
        std::error_code ec;
        std::filesystem::remove_all(dir_to_remove, ec);
        reload_instances();
        return !ec;
    }
    return false;
}

bool Launcher::is_installing() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_installing_;
}

float Launcher::install_progress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return install_progress_;
}

std::string Launcher::install_stage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return install_stage_;
}

bool Launcher::is_game_running() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return is_game_running_;
}

std::vector<std::string> Launcher::game_logs() const {
    std::lock_guard<std::mutex> lock(game_mutex_);
    return game_logs_;
}

void Launcher::clear_game_logs() {
    std::lock_guard<std::mutex> lock(game_mutex_);
    game_logs_.clear();
}

bool Launcher::is_instance_installed(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& inst : instances_) {
        if (inst.name == name) {
            return std::filesystem::exists(inst.directory / "installed.txt");
        }
    }
    return false;
}

void Launcher::update_version_manifest_async() {
    if (manifest_thread_.joinable()) {
        manifest_thread_.join();
    }
    manifest_thread_ = std::thread([this]() {
        try {
            std::string manifest_data = downloader::download_json("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json");
            if (manifest_data.empty()) return;

            auto manifest_json = json::parse(manifest_data);
            std::vector<VersionEntry> loaded;
            for (const auto& v : manifest_json["versions"]) {
                loaded.push_back({
                    v.value("id", ""),
                    v.value("type", ""),
                    v.value("releaseTime", "")
                });
            }

            if (!loaded.empty()) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    versions_ = loaded;
                }
                
                auto app_dir = make_accounts_storage_path(options_.app_name).parent_path();
                std::filesystem::create_directories(app_dir);
                std::ofstream file(app_dir / "versions.txt", std::ios::trunc);
                if (file.is_open()) {
                    for (const auto& entry : loaded) {
                        file << entry.id << ";" << entry.type << ";" << entry.release_time << "\n";
                    }
                    file.close();
                }
            }
        } catch (...) {}
    });
}

std::vector<std::pair<std::string, std::string>> Launcher::detect_system_javas() const {
    return downloader::find_installed_javas();
}

bool Launcher::install_modrinth_addons_async(const std::vector<ModrinthVersion>& versions, const std::string& type, const std::string& instance_name) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_installing_) {
            return false;
        }
        is_installing_ = true;
        install_progress_ = 0.0f;
        install_stage_ = "Starting Modrinth addon installation...";
        installing_instance_ = instance_name;
    }

    if (install_thread_.joinable()) {
        install_thread_.join();
    }

    install_thread_ = std::thread([this, versions, type, instance_name]() {
        InstanceRecord instance;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& inst : instances_) {
                if (inst.name == instance_name) {
                    instance = inst;
                    found = true;
                    break;
                }
            }
        }

        if (!found) {
            std::lock_guard<std::mutex> lock(mutex_);
            is_installing_ = false;
            status_message_ = "Error: Instance not found.";
            return;
        }

        int total = versions.size();
        int installed_count = 0;
        
        for (int i = 0; i < total; ++i) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                install_progress_ = static_cast<float>(i) / static_cast<float>(total);
                install_stage_ = "Downloading " + versions[i].filename + " (" + std::to_string(i + 1) + "/" + std::to_string(total) + ")...";
            }

            if (modrinth::install_version(versions[i], type, instance.directory)) {
                installed_count++;
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        is_installing_ = false;
        install_progress_ = 1.0f;
        status_message_ = "Successfully installed " + std::to_string(installed_count) + " / " + std::to_string(total) + " addons!";
    });

    return true;
}

bool Launcher::download_portable_java_async() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (is_installing_) {
            return false;
        }
        is_installing_ = true;
        install_progress_ = 0.0f;
        install_stage_ = "Initializing Java JRE download...";
    }

    if (install_thread_.joinable()) {
        install_thread_.join();
    }

    install_thread_ = std::thread([this]() {
        auto app_dir = make_accounts_storage_path(options_.app_name).parent_path();
        
        std::string j_bin = downloader::download_portable_java_cpp(app_dir, [this](float prog, const std::string& msg) {
            std::lock_guard<std::mutex> lock(mutex_);
            install_progress_ = prog / 100.0f;
            install_stage_ = msg;
        });

        {
            std::lock_guard<std::mutex> lock(mutex_);
            is_installing_ = false;
            install_progress_ = 1.0f;
            
            if (!j_bin.empty() && j_bin != "java") {
                settings_.java_path = j_bin;
                save_settings(settings_);
                status_message_ = "Successfully installed portable JRE 21 and configured path override!";
            } else {
                status_message_ = "Failed to download portable Java.";
            }
        }
    });

    return true;
}
