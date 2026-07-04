#pragma once

#include "app/launcher.hpp"
#include "app/modrinth.hpp"
#include <map>

class LauncherWindow {
public:
    explicit LauncherWindow(LauncherOptions options);
    ~LauncherWindow();

    int run(int argc, char** argv);

private:
    Launcher launcher_;
    int selected_instance_idx_ = -1;
    bool show_creation_ = false;
    char new_instance_name_[128] = "";
    int new_instance_version_idx_ = 0;
    int new_instance_loader_idx_ = 0;
    bool force_mock_ = false;
    bool auto_scroll_logs_ = true;

    // Settings Panel state
    bool show_settings_ = false;
    char settings_java_path_[512] = "";
    int settings_memory_mb_ = 4096;
    char settings_jvm_args_[512] = "";
    bool settings_force_mock_ = false;

    // Modrinth Panel state
    bool show_modrinth_ = false;
    char modrinth_search_[256] = "";
    int modrinth_category_idx_ = 0; // 0: mod, 1: resourcepack, 2: shaderpack
    std::vector<ModrinthProject> modrinth_results_;
    int selected_modrinth_project_idx_ = -1;
    std::vector<ModrinthVersion> modrinth_versions_;
    int selected_modrinth_version_idx_ = -1;
    std::string modrinth_status_;

    // Texture Cache
    std::map<std::string, void*> project_textures_;
    std::map<std::string, bool> download_in_progress_;

    // Java Scanner state
    std::vector<std::pair<std::string, std::string>> detected_javas_;

    // Modrinth multi-selection
    std::vector<std::string> checked_project_ids_;

    // Offline profile state
    bool show_offline_input_ = false;
    char offline_input_name_[128] = "";
};
