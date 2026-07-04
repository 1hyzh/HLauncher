#pragma once

#include "app/version_catalog.hpp"

#include <filesystem>
#include <string>
#include <vector>

struct InstanceRecord {
    std::string name;
    std::string version_id;
    std::string loader;
    std::filesystem::path directory;
};

struct LaunchPlan {
    std::filesystem::path java_executable;
    std::filesystem::path game_directory;
    std::string main_class;
    std::vector<std::string> arguments;
};

class InstanceManager {
public:
    explicit InstanceManager(std::filesystem::path root_directory);

    std::vector<InstanceRecord> load_instances() const;
    InstanceRecord create_instance(const std::string& name, const std::string& version_id, const std::string& loader);
    LaunchPlan build_launch_plan(const InstanceRecord& instance) const;
    std::vector<VersionEntry> available_versions() const;

private:
    std::filesystem::path root_directory_;
    std::filesystem::path instances_directory() const;
    std::filesystem::path game_directory_for(const std::string& name) const;
    std::string sanitize_name(const std::string& name) const;
};
