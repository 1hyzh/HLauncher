#pragma once

#include <filesystem>
#include <string>
#include <functional>

namespace downloader {

// Detects installed Java on the host machine. Returns the path to the java executable, or "java" if not found.
std::string detect_installed_java();

// Scans the system for installed JRE/JDKs and returns list of pair {description, bin_path}
std::vector<std::pair<std::string, std::string>> find_installed_javas();

// Fetches raw JSON string from web using curl command.
std::string download_json(const std::string& url);

// Installs Minecraft vanilla or modloaders for the selected version.
// progress_callback updates the percentage and stage text.
bool install_minecraft(
    const std::filesystem::path& instance_dir,
    const std::string& version_id,
    const std::string& loader,
    const std::filesystem::path& app_dir,
    std::function<void(float, const std::string&)> progress_callback
);

std::string download_portable_java_cpp(
    const std::filesystem::path& app_dir,
    const std::function<void(float, const std::string&)>& progress_callback
);

} // namespace downloader
