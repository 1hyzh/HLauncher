#pragma once

#include <string>
#include <vector>
#include <filesystem>

struct ModrinthProject {
    std::string id;
    std::string slug;
    std::string title;
    std::string description;
    std::string author;
    std::string icon_url;
    int downloads = 0;
};

struct ModrinthVersion {
    std::string id;
    std::string name;
    std::string version_number;
    std::string download_url;
    std::string filename;
};

namespace modrinth {
    // Searches Modrinth for projects matching the query, type ("mod", "resourcepack", "shaderpack"), and target Minecraft version.
    std::vector<ModrinthProject> search(const std::string& query, const std::string& type, const std::string& mc_version);
    
    // Retrieves compatible version options for a specific project and Minecraft version.
    std::vector<ModrinthVersion> get_versions(const std::string& project_id_or_slug, const std::string& mc_version);
    
    // Downloads and installs a version into the instance's appropriate subfolder.
    bool install_version(const ModrinthVersion& version, const std::string& type, const std::filesystem::path& instance_dir);
}
