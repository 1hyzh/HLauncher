#include "app/modrinth.hpp"
#include "app/downloader.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

using json = nlohmann::json;

namespace {

std::string url_encode(const std::string& value) {
    std::ostringstream escaped;
    escaped << std::hex << std::uppercase;
    for (char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
        } else if (c == ' ') {
            escaped << "%20";
        } else {
            escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
        }
    }
    return escaped.str();
}

bool download_file_raw(const std::string& url, const std::filesystem::path& dest) {
    std::filesystem::create_directories(dest.parent_path());
    std::string command = "curl -s -L -f -o \"" + dest.string() + "\" \"" + url + "\"";
    int result = std::system(command.c_str());
    return result == 0;
}

} // namespace

namespace modrinth {

std::vector<ModrinthProject> search(const std::string& query, const std::string& type, const std::string& mc_version) {
    std::vector<ModrinthProject> results;
    try {
        std::string encoded_query = url_encode(query);
        
        // Modrinth facets filter: [["project_type:<type>"],["versions:<mc_version>"]]
        std::string facets = "";
        if (!type.empty() && !mc_version.empty()) {
            facets = url_encode("[[\"project_type:" + type + "\"],[\"versions:" + mc_version + "\"]]");
        } else if (!type.empty()) {
            facets = url_encode("[[\"project_type:" + type + "\"]]");
        } else if (!mc_version.empty()) {
            facets = url_encode("[[\"versions:" + mc_version + "\"]]");
        }

        std::string url = "https://api.modrinth.com/v2/search?query=" + encoded_query;
        if (!facets.empty()) {
            url += "&facets=" + facets;
        }
        url += "&limit=20";

        std::string data = downloader::download_json(url);
        if (data.empty()) return results;

        auto res_json = json::parse(data);
        if (res_json.contains("hits")) {
            for (const auto& hit : res_json["hits"]) {
                ModrinthProject proj;
                proj.id = hit.value("project_id", "");
                proj.slug = hit.value("slug", "");
                proj.title = hit.value("title", "");
                proj.description = hit.value("description", "");
                proj.author = hit.value("author", "");
                proj.icon_url = hit.value("icon_url", "");
                proj.downloads = hit.value("downloads", 0);
                results.push_back(proj);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Modrinth Search Error: " << e.what() << std::endl;
    }
    return results;
}

std::vector<ModrinthVersion> get_versions(const std::string& project_id_or_slug, const std::string& mc_version) {
    std::vector<ModrinthVersion> results;
    try {
        std::string url = "https://api.modrinth.com/v2/project/" + url_encode(project_id_or_slug) + "/version";
        std::string data = downloader::download_json(url);
        if (data.empty()) return results;

        auto res_json = json::parse(data);
        for (const auto& item : res_json) {
            // Filter by compatible game versions if specified
            if (!mc_version.empty()) {
                bool compatible = false;
                if (item.contains("game_versions")) {
                    for (const auto& gv : item["game_versions"]) {
                        if (gv == mc_version) {
                            compatible = true;
                            break;
                        }
                    }
                }
                if (!compatible) {
                    continue;
                }
            }

            if (item.contains("files") && !item["files"].empty()) {
                const auto& file = item["files"][0];
                ModrinthVersion ver;
                ver.id = item.value("id", "");
                ver.name = item.value("name", "");
                ver.version_number = item.value("version_number", "");
                ver.download_url = file.value("url", "");
                ver.filename = file.value("filename", "");
                if (!ver.download_url.empty() && !ver.filename.empty()) {
                    results.push_back(ver);
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Modrinth Versions Error: " << e.what() << std::endl;
    }
    return results;
}

bool install_version(const ModrinthVersion& version, const std::string& type, const std::filesystem::path& instance_dir) {
    std::string folder_name = "mods";
    if (type == "resourcepack") {
        folder_name = "resourcepacks";
    } else if (type == "shaderpack") {
        folder_name = "shaderpacks";
    }

    std::filesystem::path dest = instance_dir / folder_name / version.filename;
    return download_file_raw(version.download_url, dest);
}

} // namespace modrinth
