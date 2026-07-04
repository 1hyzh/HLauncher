#include "app/version_catalog.hpp"
#include <fstream>
#include <sstream>

std::vector<VersionEntry> VersionCatalog::load_default_versions() const {
    return {
        {"1.21.1", "release", "2024-09-17"},
        {"1.21", "release", "2024-06-13"},
        {"1.20.6", "release", "2024-04-29"},
        {"1.20.1", "release", "2023-06-12"},
        {"1.19.4", "release", "2023-03-14"},
        {"1.18.2", "release", "2022-02-28"}
    };
}

std::vector<VersionEntry> VersionCatalog::load_from_file(const std::filesystem::path& file_path) const {
    std::vector<VersionEntry> entries;
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return entries;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) {
            continue;
        }

        std::stringstream ss(line);
        std::string id, type, release_time;
        if (std::getline(ss, id, ';') &&
            std::getline(ss, type, ';') &&
            std::getline(ss, release_time, ';')) {
            entries.push_back({id, type, release_time});
        }
    }
    return entries;
}
