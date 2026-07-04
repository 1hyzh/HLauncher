#include "app/downloader.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <cctype>
#include <atomic>
#include <thread>
#include <mutex>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define popen _popen
#define pclose _pclose
#define EXE_EXT ".exe"
#else
#include <unistd.h>
#include <sys/types.h>
#define EXE_EXT ""
#endif

using json = nlohmann::json;

namespace {

bool download_file_cpp(const std::string& url, const std::filesystem::path& dest) {
    std::filesystem::create_directories(dest.parent_path());
    if (std::filesystem::exists(dest) && std::filesystem::file_size(dest) > 0) {
        return true;
    }
    std::string command = "curl -s -L -f -o \"" + dest.string() + "\" \"" + url + "\"";
    int result = std::system(command.c_str());
    return result == 0;
}

std::string download_json_cpp(const std::string& url) {
    std::string command = "curl -s -L -f \"" + url + "\"";
    FILE* fp = popen(command.c_str(), "r");
    if (!fp) return "";
    
    std::string data;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
        data += buffer;
    }
    pclose(fp);
    return data;
}

void extract_natives_cpp(const std::filesystem::path& jar_path, const std::filesystem::path& natives_dir) {
    std::filesystem::create_directories(natives_dir);
#if defined(_WIN32)
    std::string cmd = "powershell -Command \"Expand-Archive -Force -Path '" + jar_path.string() + "' -DestinationPath '" + natives_dir.string() + "'\"";
    std::system(cmd.c_str());
#else
    std::string cmd = "unzip -o -j \"" + jar_path.string() + "\" \"*.dylib\" \"*.jnilib\" \"*.so\" -d \"" + natives_dir.string() + "\" >/dev/null 2>&1";
    std::system(cmd.c_str());
#endif
}

void extract_tar_gz_cpp(const std::filesystem::path& archive_path, const std::filesystem::path& dest_dir) {
    std::filesystem::create_directories(dest_dir);
#if defined(_WIN32)
    std::string cmd = "powershell -Command \"Expand-Archive -Force -Path '" + archive_path.string() + "' -DestinationPath '" + dest_dir.string() + "'\"";
    std::system(cmd.c_str());
#else
    std::string cmd = "tar -C \"" + dest_dir.string() + "\" -zxf \"" + archive_path.string() + "\"";
    std::system(cmd.c_str());
#endif
}

bool is_library_allowed_cpp(const json& lib) {
    if (!lib.contains("rules")) {
        return true;
    }
    bool allowed = false;
    for (const auto& rule : lib["rules"]) {
        std::string action = rule["action"];
        bool match = true;
        if (rule.contains("os")) {
            std::string os_name = rule["os"]["name"];
#if defined(__APPLE__)
            if (os_name != "osx") match = false;
#elif defined(_WIN32)
            if (os_name != "windows") match = false;
#else
            if (os_name != "linux") match = false;
#endif
        }
        if (match) {
            allowed = (action == "allow");
        }
    }
    return allowed;
}

} // namespace

namespace downloader {

std::string download_portable_java_cpp(const std::filesystem::path& app_dir, const std::function<void(float, const std::string&)>& progress_callback) {
    std::filesystem::path runtime_dir = app_dir / "runtime";
    std::filesystem::create_directories(runtime_dir);

    std::string os_name = "linux";
#if defined(__APPLE__)
    os_name = "mac";
#elif defined(_WIN32)
    os_name = "windows";
#endif

    std::string arch = "x64";
#if defined(__arm64__) || defined(__aarch64__)
    arch = "aarch64";
#endif

    std::string ext = (os_name == "windows") ? "zip" : "tar.gz";
    std::filesystem::path java_dir = runtime_dir / "jre21";
    
    std::filesystem::path java_bin = java_dir;
#if defined(__APPLE__)
    java_bin = java_dir / "Contents" / "Home" / "bin" / "java";
#else
    java_bin = java_dir / "bin" / ("java" + std::string(EXE_EXT));
#endif

    if (std::filesystem::exists(java_bin)) {
        return java_bin.string();
    }

    progress_callback(85.0f, "Downloading portable Java 21 JRE...");
    std::string url = "https://api.adoptium.net/v3/binary/latest/21/ga/" + os_name + "/" + arch + "/jre/hotspot/normal/eclipse";
    std::filesystem::path archive_path = runtime_dir / ("jre_temp." + ext);
    
    if (!download_file_cpp(url, archive_path)) {
        return "java";
    }

    progress_callback(92.0f, "Extracting portable Java 21 runtime...");
    if (ext == "zip") {
#if defined(_WIN32)
        extract_natives_cpp(archive_path, java_dir);
#endif
    } else {
        extract_tar_gz_cpp(archive_path, runtime_dir);
    }

    // Rename the extracted folder (e.g. jdk-21.0.2+13-jre) to jre21
    std::filesystem::path extracted_folder;
    for (const auto& entry : std::filesystem::directory_iterator(runtime_dir)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            if ((name.rfind("jdk", 0) == 0 || name.rfind("eclipse", 0) == 0 || name.find("jre") != std::string::npos) && name != "jre21") {
                extracted_folder = entry.path();
                break;
            }
        }
    }

    if (!extracted_folder.empty()) {
        if (std::filesystem::exists(java_dir)) {
            std::filesystem::remove_all(java_dir);
        }
        std::filesystem::rename(extracted_folder, java_dir);
    }

    if (std::filesystem::exists(archive_path)) {
        std::filesystem::remove(archive_path);
    }

    if (std::filesystem::exists(java_bin)) {
#if !defined(_WIN32)
        std::string chmod_cmd = "chmod +x \"" + java_bin.string() + "\"";
        std::system(chmod_cmd.c_str());
        // Set exec permission on other helper binaries
        std::string chmod_all = "chmod +x \"" + java_bin.parent_path().string() + "\"/* >/dev/null 2>&1";
        std::system(chmod_all.c_str());
#endif
        return java_bin.string();
    }

    return "java";
}

// Continuation of downloader namespace

std::string download_json(const std::string& url) {
    return download_json_cpp(url);
}

std::string detect_installed_java() {
    // 1. Check JAVA_HOME environment variable
    if (const char* java_home = std::getenv("JAVA_HOME")) {
        std::filesystem::path p(java_home);
        std::filesystem::path bin = p / "bin" / ("java" + std::string(EXE_EXT));
        if (std::filesystem::exists(bin)) {
            return bin.string();
        }
    }

    // 2. Scan for installed Javas
    auto javas = find_installed_javas();
    if (!javas.empty()) {
        return javas.front().second;
    }

    return "";
}

std::vector<std::pair<std::string, std::string>> find_installed_javas() {
    std::vector<std::pair<std::string, std::string>> list;

#if defined(__APPLE__)
    // 1. Run /usr/libexec/java_home -V and parse JVMs
    FILE* fp = popen("/usr/libexec/java_home -V 2>&1", "r");
    if (fp) {
        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), fp) != nullptr) {
            std::string line(buffer);
            size_t slash = line.find('/');
            if (slash != std::string::npos) {
                std::string path = line.substr(slash);
                if (!path.empty() && path.back() == '\n') path.pop_back();
                if (!path.empty() && path.back() == '\r') path.pop_back();
                
                std::filesystem::path p(path);
                std::filesystem::path java_bin = p / "bin" / "java";
                if (std::filesystem::exists(java_bin)) {
                    std::string desc = "Java VM";
                    size_t first_quote = line.find('"');
                    if (first_quote != std::string::npos) {
                        size_t second_quote = line.find('"', first_quote + 1);
                        if (second_quote != std::string::npos) {
                            desc = line.substr(first_quote + 1, second_quote - first_quote - 1);
                        }
                    } else {
                        desc = p.parent_path().parent_path().filename().string();
                        if (desc.empty()) desc = p.filename().string();
                    }
                    
                    bool dup = false;
                    for (const auto& item : list) {
                        if (item.second == java_bin.string()) {
                            dup = true;
                            break;
                        }
                    }
                    if (!dup) {
                        list.push_back({desc, java_bin.string()});
                    }
                }
            }
        }
        pclose(fp);
    }

    // 2. Scan standard directories manually
    std::vector<std::filesystem::path> common_dirs = {
        "/Library/Java/JavaVirtualMachines",
        "/System/Library/Java/JavaVirtualMachines",
        "/opt/homebrew/opt",
        "/usr/local/opt"
    };

    for (const auto& base : common_dirs) {
        if (std::filesystem::exists(base)) {
            try {
                for (const auto& entry : std::filesystem::directory_iterator(base)) {
                    if (entry.is_directory()) {
                        std::filesystem::path java_bin = entry.path() / "Contents" / "Home" / "bin" / "java";
                        std::string name = entry.path().filename().string();
                        if (std::filesystem::exists(java_bin)) {
                            bool dup = false;
                            for (const auto& item : list) {
                                if (item.second == java_bin.string()) { dup = true; break; }
                            }
                            if (!dup) list.push_back({name, java_bin.string()});
                        }
                        
                        std::filesystem::path homebrew_bin = entry.path() / "bin" / "java";
                        if (std::filesystem::exists(homebrew_bin)) {
                            bool dup = false;
                            for (const auto& item : list) {
                                if (item.second == homebrew_bin.string()) { dup = true; break; }
                            }
                            if (!dup) list.push_back({name, homebrew_bin.string()});
                        }
                    }
                }
            } catch (...) {}
        }
    }
#elif defined(_WIN32)
    // Windows checks registry
    // ...
#endif

    return list;
}

bool install_minecraft(
    const std::filesystem::path& instance_dir,
    const std::string& version_id,
    const std::string& loader,
    const std::filesystem::path& app_dir,
    std::function<void(float, const std::string&)> progress_callback
) {
    try {
        progress_callback(5.0f, "Fetching Mojang version manifest...");
        std::string manifest_data = download_json_cpp("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json");
        if (manifest_data.empty()) {
            return false;
        }

        auto manifest_json = json::parse(manifest_data);
        std::string version_url;
        for (const auto& v : manifest_json["versions"]) {
            if (v["id"] == version_id) {
                version_url = v["url"];
                break;
            }
        }

        if (version_url.empty()) {
            return false;
        }

        progress_callback(10.0f, "Downloading Minecraft version details...");
        std::filesystem::path version_json_dest = instance_dir / (version_id + ".json");
        if (!download_file_cpp(version_url, version_json_dest)) {
            return false;
        }

        std::ifstream f_in(version_json_dest);
        if (!f_in.is_open()) return false;
        json version_json = json::parse(f_in);
        f_in.close();

        progress_callback(20.0f, "Downloading Minecraft client jar...");
        std::string client_url = version_json["downloads"]["client"]["url"];
        std::filesystem::path client_dest = instance_dir / "client.jar";
        if (!download_file_cpp(client_url, client_dest)) {
            return false;
        }

        progress_callback(30.0f, "Resolving and downloading library jars...");
        std::vector<std::string> classpath = { client_dest.string() };
        std::filesystem::path natives_dir = instance_dir / "natives";

        const auto& libraries = version_json["libraries"];
        for (const auto& lib : libraries) {
            if (is_library_allowed_cpp(lib)) {
                const auto& downloads = lib["downloads"];
                if (downloads.contains("artifact")) {
                    std::string lib_url = downloads["artifact"]["url"];
                    std::string lib_path = downloads["artifact"]["path"];
                    std::filesystem::path dest_path = app_dir / "libraries" / lib_path;
                    if (download_file_cpp(lib_url, dest_path)) {
                        classpath.push_back(dest_path.string());
                    }
                }

                // Check natives
                if (downloads.contains("classifiers")) {
                    std::string native_key;
#if defined(__APPLE__)
                    if (downloads["classifiers"].contains("natives-osx")) native_key = "natives-osx";
                    else if (downloads["classifiers"].contains("natives-macos")) native_key = "natives-macos";
#elif defined(_WIN32)
                    if (downloads["classifiers"].contains("natives-windows")) native_key = "natives-windows";
#else
                    if (downloads["classifiers"].contains("natives-linux")) native_key = "natives-linux";
#endif
                    if (!native_key.empty()) {
                        std::string native_url = downloads["classifiers"][native_key]["url"];
                        std::string native_path = downloads["classifiers"][native_key]["path"];
                        std::filesystem::path dest_path = app_dir / "libraries" / native_path;
                        if (download_file_cpp(native_url, dest_path)) {
                            extract_natives_cpp(dest_path, natives_dir);
                        }
                    }
                }
            }
        }

        std::string main_class = version_json.value("mainClass", "net.minecraft.client.main.Main");

        // 6. Handle Fabric Installation
        if (loader == "fabric") {
            progress_callback(50.0f, "Fetching Fabric profile configuration...");
            std::string fabric_meta_url = "https://meta.fabricmc.net/v2/versions/loader/" + version_id;
            std::string meta_data = download_json_cpp(fabric_meta_url);
            if (!meta_data.empty()) {
                auto loaders_json = json::parse(meta_data);
                if (!loaders_json.empty()) {
                    std::string loader_version = loaders_json[0]["loader"]["version"];
                    std::string profile_url = "https://meta.fabricmc.net/v2/versions/loader/" + version_id + "/" + loader_version + "/profile/json";
                    std::string profile_data = download_json_cpp(profile_url);
                    if (!profile_data.empty()) {
                        auto fabric_profile = json::parse(profile_data);
                        main_class = fabric_profile.value("mainClass", "net.fabricmc.loader.impl.launch.knot.KnotClient");
                        
                        progress_callback(60.0f, "Downloading Fabric library dependencies...");
                        for (const auto& flib : fabric_profile["libraries"]) {
                            std::string name = flib["name"];
                            std::string base_url = flib.value("url", "https://maven.fabricmc.net/");
                            
                            // Parse Maven Coordinates: group:artifact:version
                            std::stringstream ss(name);
                            std::string group, artifact_id, ver;
                            if (std::getline(ss, group, ':') &&
                                std::getline(ss, artifact_id, ':') &&
                                std::getline(ss, ver, ':')) {
                                
                                std::replace(group.begin(), group.end(), '.', '/');
                                std::string flib_path = group + "/" + artifact_id + "/" + ver + "/" + artifact_id + "-" + ver + ".jar";
                                std::string flib_url = base_url + flib_path;
                                std::filesystem::path dest_path = app_dir / "libraries" / flib_path;
                                if (download_file_cpp(flib_url, dest_path)) {
                                    classpath.push_back(dest_path.string());
                                }
                            }
                        }
                    }
                }
            }
        }

        // 7. Download Essential Assets
        std::string asset_index_id = "legacy";
        if (version_json.contains("assetIndex")) {
            asset_index_id = version_json["assetIndex"].value("id", "legacy");
            std::string asset_index_url = version_json["assetIndex"]["url"];
            
            progress_callback(70.0f, "Downloading Minecraft assets catalog...");
            std::filesystem::path index_dest = app_dir / "assets" / "indexes" / (asset_index_id + ".json");
            if (download_file_cpp(asset_index_url, index_dest)) {
                std::ifstream f_assets(index_dest);
                if (f_assets.is_open()) {
                    auto assets_json = json::parse(f_assets);
                    f_assets.close();
                    
                    std::vector<std::pair<std::filesystem::path, std::string>> pending_assets;
                    if (assets_json.contains("objects")) {
                        for (const auto& [path, obj] : assets_json["objects"].items()) {
                            if (obj.contains("hash")) {
                                std::string hash = obj["hash"];
                                std::string prefix = hash.substr(0, 2);
                                std::filesystem::path dest = app_dir / "assets" / "objects" / prefix / hash;
                                if (!std::filesystem::exists(dest)) {
                                    pending_assets.push_back({dest, hash});
                                }
                            }
                        }
                    }

                    if (!pending_assets.empty()) {
                        progress_callback(75.0f, "Downloading Minecraft game assets...");
                        std::mutex queue_mutex;
                        std::size_t next_idx = 0;
                        std::atomic<int> completed_count(0);
                        int total_pending = pending_assets.size();
                        
                        unsigned int num_threads = 16;
                        std::vector<std::thread> workers;
                        for (unsigned int t = 0; t < num_threads; ++t) {
                            workers.push_back(std::thread([&]() {
                                while (true) {
                                    std::size_t idx = 0;
                                    {
                                        std::lock_guard<std::mutex> lock(queue_mutex);
                                        if (next_idx >= pending_assets.size()) {
                                            break;
                                        }
                                        idx = next_idx++;
                                    }
                                    
                                    const auto& item = pending_assets[idx];
                                    std::filesystem::create_directories(item.first.parent_path());
                                    std::string prefix = item.second.substr(0, 2);
                                    std::string url = "https://resources.download.minecraft.net/" + prefix + "/" + item.second;
                                    download_file_cpp(url, item.first);
                                    
                                    {
                                        std::lock_guard<std::mutex> lock(queue_mutex);
                                        int done = ++completed_count;
                                        if (done % 50 == 0 || done == total_pending) {
                                            float pct = 75.0f + (static_cast<float>(done) / static_cast<float>(total_pending)) * 15.0f;
                                            progress_callback(pct, "Downloading assets (" + std::to_string(done) + "/" + std::to_string(total_pending) + ")...");
                                        }
                                    }
                                }
                            }));
                        }
                        
                        for (auto& worker : workers) {
                            if (worker.joinable()) {
                                worker.join();
                            }
                        }
                    }
                }
            }
        }

        // 8. Detect/Install Java
        progress_callback(80.0f, "Configuring Java execution path...");
        std::string java_bin = detect_installed_java();
        if (java_bin.empty()) {
            java_bin = download_portable_java_cpp(app_dir, progress_callback);
        }

        // 9. Write launch parameters to flat file
        progress_callback(95.0f, "Creating final launch plan configurations...");
        
        std::string variables[11][2] = {
            {"auth_player_name", "Player"},
            {"version_name", version_id},
            {"game_directory", instance_dir.string()},
            {"assets_root", (app_dir / "assets").string()},
            {"assets_index_name", asset_index_id},
            {"auth_uuid", "offline-uuid"},
            {"auth_access_token", "0"},
            {"user_type", "mojang"},
            {"version_type", version_json.value("type", "release")},
            {"resolution_width", "854"},
            {"resolution_height", "480"}
        };

        std::vector<std::string> game_args;
        if (version_json.contains("arguments") && version_json["arguments"].contains("game")) {
            for (const auto& item : version_json["arguments"]["game"]) {
                if (item.is_string()) {
                    game_args.push_back(item);
                } else if (item.is_object() && item.contains("value")) {
                    if (item["value"].is_array()) {
                        for (const auto& v : item["value"]) {
                            game_args.push_back(v);
                        }
                    } else if (item["value"].is_string()) {
                        game_args.push_back(item["value"]);
                    }
                }
            }
        } else if (version_json.contains("minecraftArguments")) {
            std::string raw_args = version_json["minecraftArguments"];
            std::stringstream ss(raw_args);
            std::string tmp;
            while (ss >> tmp) {
                game_args.push_back(tmp);
            }
        }

        std::vector<std::string> resolved_args;
        for (auto arg : game_args) {
            if (arg == "--demo") {
                continue;
            }
            for (int i = 0; i < 11; ++i) {
                std::string target = "${" + variables[i][0] + "}";
                size_t pos = 0;
                while ((pos = arg.find(target, pos)) != std::string::npos) {
                    arg.replace(pos, target.length(), variables[i][1]);
                    pos += variables[i][1].length();
                }
            }
            resolved_args.push_back(arg);
        }

        std::ofstream file(instance_dir / "launch_args.txt", std::ios::trunc);
        if (file.is_open()) {
            file << java_bin << "\n";
            file << main_class << "\n";
            std::string sep = ";";
#if !defined(_WIN32)
            sep = ":";
#endif
            std::string cp_str;
            for (size_t i = 0; i < classpath.size(); ++i) {
                cp_str += classpath[i];
                if (i + 1 < classpath.size()) cp_str += sep;
            }
            file << cp_str << "\n";
            file << asset_index_id << "\n";
            for (const auto& arg : resolved_args) {
                file << arg << "\n";
            }
            file.close();
        }

        progress_callback(100.0f, "Ready to Launch!");
        return true;

    } catch (const std::exception& e) {
        std::cerr << "C++ Installer error: " << e.what() << std::endl;
        return false;
    }
}

} // namespace downloader
