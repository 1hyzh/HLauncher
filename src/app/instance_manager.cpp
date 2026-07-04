#include "app/instance_manager.hpp"

#include "app/version_catalog.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace {

std::string trim_copy(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} // namespace

InstanceManager::InstanceManager(std::filesystem::path root_directory)
    : root_directory_(std::move(root_directory)) {}

std::filesystem::path InstanceManager::instances_directory() const {
    return root_directory_ / "instances";
}

std::filesystem::path InstanceManager::game_directory_for(const std::string& name) const {
    return instances_directory() / sanitize_name(name);
}

std::string InstanceManager::sanitize_name(const std::string& name) const {
    std::string sanitized;
    sanitized.reserve(name.size());
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' ) {
            sanitized.push_back(ch);
        } else if (ch == ' ') {
            sanitized.push_back('_');
        }
    }
    if (sanitized.empty()) {
        sanitized = "instance";
    }
    return sanitized;
}

std::vector<InstanceRecord> InstanceManager::load_instances() const {
    std::vector<InstanceRecord> instances;
    const auto dir = instances_directory();
    if (!std::filesystem::exists(dir)) {
        return instances;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
        if (!entry.is_directory()) {
            continue;
        }

        const auto manifest_path = entry.path() / "instance.txt";
        std::ifstream file(manifest_path);
        if (!file.is_open()) {
            continue;
        }

        std::string name;
        std::string version_id;
        std::string loader;
        std::getline(file, name);
        std::getline(file, version_id);
        std::getline(file, loader);

        instances.push_back(InstanceRecord{
            trim_copy(name),
            trim_copy(version_id),
            trim_copy(loader),
            entry.path()
        });
    }

    return instances;
}

InstanceRecord InstanceManager::create_instance(const std::string& name, const std::string& version_id, const std::string& loader) {
    const auto directory = game_directory_for(name);
    std::filesystem::create_directories(directory);

    std::ofstream file(directory / "instance.txt", std::ios::trunc);
    file << name << '\n' << version_id << '\n' << loader << '\n';

    return InstanceRecord{name, version_id, loader, directory};
}

LaunchPlan InstanceManager::build_launch_plan(const InstanceRecord& instance) const {
    LaunchPlan plan;
    plan.game_directory = instance.directory;
#if defined(_WIN32)
    plan.java_executable = "javaw.exe";
#else
    plan.java_executable = "java";
#endif
    plan.main_class = "net.minecraft.client.main.Main";
    plan.arguments = {
        "--username", "Player",
        "--version", instance.version_id,
        "--gameDir", instance.directory.string(),
        "--assetsDir", (root_directory_ / "assets").string(),
        "--assetIndex", instance.version_id,
        "--uuid", "00000000-0000-0000-0000-000000000000",
        "--accessToken", "0"
    };
    if (instance.loader == "fabric") {
        plan.main_class = "net.fabricmc.loader.impl.launch.knot.KnotClient";
        plan.arguments.push_back("--fabric-loader");
    } else if (instance.loader == "forge") {
        plan.main_class = "net.minecraftforge.client.loading.ClientModLoader";
        plan.arguments.push_back("--forge-loader");
    }
    return plan;
}

std::vector<VersionEntry> InstanceManager::available_versions() const {
    VersionCatalog catalog;
    return catalog.load_default_versions();
}
