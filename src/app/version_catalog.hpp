#pragma once

#include <filesystem>
#include <string>
#include <vector>

struct VersionEntry {
    std::string id;
    std::string type;
    std::string release_time;
};

class VersionCatalog {
public:
    std::vector<VersionEntry> load_default_versions() const;
    std::vector<VersionEntry> load_from_file(const std::filesystem::path& file_path) const;
};
