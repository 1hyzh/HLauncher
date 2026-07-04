#include "ui/launcher_window.hpp"

#if defined(__APPLE__)
#include "platform/macos_url.hpp"
#endif

#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include "ui/stb_image.h"
#include <fstream>
#include <thread>

#include <utility>
#include <vector>
#include <string>

namespace {

SDL_Texture* load_texture_from_file(SDL_Renderer* renderer, const std::filesystem::path& file_path) {
    int width, height, channels;
    unsigned char* data = stbi_load(file_path.string().c_str(), &width, &height, &channels, 4);
    if (!data) {
        return nullptr;
    }
    
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA32,
        SDL_TEXTUREACCESS_STATIC,
        width,
        height
    );
    
    if (texture) {
        SDL_UpdateTexture(texture, nullptr, data, width * 4);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    
    stbi_image_free(data);
    return texture;
}

void apply_premium_theme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(20.0f, 20.0f);
    style.FramePadding = ImVec2(14.0f, 8.0f);
    style.CellPadding = ImVec2(10.0f, 10.0f);
    style.ItemSpacing = ImVec2(12.0f, 12.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 8.0f);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 6.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.55f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.14f, 0.19f, 0.90f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.12f, 0.14f, 0.20f, 0.95f);
    colors[ImGuiCol_Border]                 = ImVec4(0.20f, 0.24f, 0.32f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.16f, 0.20f, 0.28f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.22f, 0.27f, 0.38f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.34f, 0.46f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.09f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.09f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.09f, 0.11f, 0.15f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.12f, 0.14f, 0.19f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.12f, 0.14f, 0.19f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.26f, 0.30f, 0.40f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.32f, 0.38f, 0.50f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.38f, 0.45f, 0.60f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.00f, 0.85f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.00f, 0.85f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]        = ImVec4(0.00f, 1.00f, 0.85f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.18f, 0.55f, 0.95f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.25f, 0.65f, 1.00f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.12f, 0.45f, 0.85f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.24f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.24f, 0.32f, 0.48f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.28f, 0.38f, 0.56f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.20f, 0.24f, 0.32f, 1.00f);
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.28f, 0.38f, 0.56f, 1.00f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.00f, 0.85f, 0.70f, 1.00f);
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.18f, 0.55f, 0.95f, 0.30f);
    colors[ImGuiCol_NavHighlight]           = ImVec4(0.00f, 0.85f, 0.70f, 1.00f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.05f, 0.07f, 0.10f, 0.60f);
}

bool is_loader_compatible(const std::string& version_id, const std::string& version_type, const std::string& loader) {
    if (loader == "vanilla") {
        return true;
    }
    
    if (loader == "fabric") {
        if (version_id.size() >= 3 && std::isdigit(version_id[0]) && std::isdigit(version_id[1]) && version_id[2] == 'w') {
            try {
                int year = std::stoi(version_id.substr(0, 2));
                return year >= 19;
            } catch (...) {
                return false;
            }
        }
        
        if (version_id.rfind("1.", 0) == 0) {
            auto first_dot = version_id.find('.');
            auto second_dot = version_id.find('.', first_dot + 1);
            std::string minor_str = (second_dot == std::string::npos) 
                ? version_id.substr(first_dot + 1)
                : version_id.substr(first_dot + 1, second_dot - first_dot - 1);
            try {
                int minor = std::stoi(minor_str);
                return minor >= 14;
            } catch (...) {
                return false;
            }
        }
        return false;
    }
    
    if (loader == "forge") {
        if (version_type != "release") {
            return false;
        }
        
        if (version_id.rfind("1.", 0) == 0) {
            auto first_dot = version_id.find('.');
            auto second_dot = version_id.find('.', first_dot + 1);
            std::string minor_str = (second_dot == std::string::npos) 
                ? version_id.substr(first_dot + 1)
                : version_id.substr(first_dot + 1, second_dot - first_dot - 1);
            try {
                int minor = std::stoi(minor_str);
                return minor >= 1;
            } catch (...) {
                return false;
            }
        }
        return false;
    }
    
    return false;
}

} // namespace

LauncherWindow::LauncherWindow(LauncherOptions options)
    : launcher_(std::move(options)) {}

LauncherWindow::~LauncherWindow() {
    for (auto& pair : project_textures_) {
        if (pair.second) {
            SDL_DestroyTexture(static_cast<SDL_Texture*>(pair.second));
        }
    }
}

int LauncherWindow::run(int argc, char** argv) {
    const char* categories[] = { "Mods", "Resource Packs", "Shaderpacks" };
    const char* types[] = { "mod", "resourcepack", "shaderpack" };

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return 1;
    }

#if defined(__APPLE__)
    install_macos_url_handler(&launcher_);
#endif

    SDL_Window* window = SDL_CreateWindow(
        "HLauncher",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1440,
        960,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        SDL_Quit();
        return 1;
    }

    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    SDL_SetWindowMinimumSize(window, 1100, 720);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.FontGlobalScale = 1.1f;

    apply_premium_theme();

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    if (!launcher_.initialize_runtime(argc, argv)) {
        ImGui_ImplSDLRenderer2_Shutdown();
        ImGui::DestroyContext();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 0;
    }

    // Auto-select first instance if available
    {
        const auto insts = launcher_.instances();
        if (!insts.empty()) {
            selected_instance_idx_ = 0;
        }
    }

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
                running = false;
            }
        }

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("HLauncher", nullptr,
                     ImGuiWindowFlags_NoDecoration |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus);

        // Sidebar - Left Column
        ImGui::BeginChild("Sidebar", ImVec2(340.0f, 0.0f), true, ImGuiWindowFlags_None);
        
        ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.70f, 1.00f), "HLAUNCHER");
        ImGui::TextDisabled("Cross-platform Minecraft Launcher");
        ImGui::Separator();

        // 1. Accounts Panel
        ImGui::TextDisabled("ACCOUNTS");
        const std::vector<AccountRecord> accounts = launcher_.accounts();
        
        if (show_offline_input_) {
            ImGui::Text("Add Offline Profile:");
            ImGui::InputText("Username", offline_input_name_, sizeof(offline_input_name_));
            if (ImGui::Button("Save Profile", ImVec2(120, 30))) {
                if (offline_input_name_[0] != '\0') {
                    launcher_.add_offline_account(offline_input_name_);
                    show_offline_input_ = false;
                    offline_input_name_[0] = '\0';
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 30))) {
                show_offline_input_ = false;
            }
        } else {
            if (!accounts.empty()) {
                const auto& active = accounts.front();
                ImGui::BeginChild("AccountCard", ImVec2(0.0f, 90.0f), true);
                ImGui::Text("Active Profile:");
                ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.70f, 1.00f), "%s", active.display_name.c_str());
                ImGui::TextDisabled("Status: %s", active.status.c_str());
                ImGui::EndChild();

                if (accounts.size() > 1) {
                    std::vector<const char*> account_names;
                    for (const auto& acc : accounts) {
                        account_names.push_back(acc.display_name.c_str());
                    }
                    int active_idx = 0;
                    ImGui::SetNextItemWidth(-1);
                    if (ImGui::Combo("##ActiveProfileSel", &active_idx, account_names.data(), account_names.size())) {
                        launcher_.set_active_account(active_idx);
                    }
                }
                
                if (ImGui::Button("Sign in with Microsoft", ImVec2(-1, 35))) {
                    launcher_.start_auth_flow();
                }
                if (ImGui::Button("Play Offline (Create Profile)", ImVec2(-1, 35))) {
                    show_offline_input_ = true;
                    offline_input_name_[0] = '\0';
                }
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
                if (ImGui::Button("Remove Profile", ImVec2(-1, 35))) {
                    launcher_.remove_active_account();
                }
                ImGui::PopStyleColor(3);
            } else {
                ImGui::TextDisabled("No profiles added yet.");
                if (ImGui::Button("Sign in with Microsoft", ImVec2(-1, 45))) {
                    launcher_.start_auth_flow();
                }
                if (ImGui::Button("Play Offline (Create Profile)", ImVec2(-1, 45))) {
                    show_offline_input_ = true;
                    offline_input_name_[0] = '\0';
                }
            }
        }
        ImGui::Spacing();
        ImGui::Separator();

        // 2. Instances List Panel
        ImGui::TextDisabled("INSTANCES");
        const std::vector<InstanceRecord> instances = launcher_.instances();
        
        ImGui::BeginChild("InstancesList", ImVec2(0.0f, -250.0f), false);
        if (instances.empty()) {
            ImGui::TextDisabled("No instances configured.");
        } else {
            for (std::size_t i = 0; i < instances.size(); ++i) {
                bool is_selected = (selected_instance_idx_ == static_cast<int>(i)) && !show_creation_ && !show_settings_ && !show_modrinth_;
                std::string label = instances[i].name + " [" + instances[i].version_id + "]";
                if (instances[i].loader != "vanilla") {
                    label += " (" + instances[i].loader + ")";
                }
                
                if (ImGui::Selectable(label.c_str(), is_selected, ImGuiSelectableFlags_None, ImVec2(0, 36))) {
                    selected_instance_idx_ = static_cast<int>(i);
                    show_creation_ = false;
                    show_settings_ = false;
                    show_modrinth_ = false;
                }
            }
        }
        ImGui::EndChild();

        if (ImGui::Button("+ New Instance", ImVec2(-1, 45))) {
            show_creation_ = true;
            show_settings_ = false;
            show_modrinth_ = false;
            new_instance_name_[0] = '\0';
            new_instance_version_idx_ = 0;
            new_instance_loader_idx_ = 0;
        }

        if (ImGui::Button("Launcher Settings", ImVec2(-1, 45))) {
            show_settings_ = true;
            show_creation_ = false;
            show_modrinth_ = false;
            
            LauncherSettings s = launcher_.settings();
            std::snprintf(settings_java_path_, sizeof(settings_java_path_), "%s", s.java_path.c_str());
            settings_memory_mb_ = s.allocated_memory_mb;
            std::snprintf(settings_jvm_args_, sizeof(settings_jvm_args_), "%s", s.jvm_args.c_str());
            settings_force_mock_ = s.force_mock;
        }

        if (ImGui::Button("Quit", ImVec2(-1, 35))) {
            running = false;
        }

        ImGui::EndChild(); // End Sidebar

        ImGui::SameLine();

        // Dashboard - Right Column
        ImGui::BeginChild("Dashboard", ImVec2(0.0f, 0.0f), false);

        // Status bar on the top
        ImGui::TextDisabled("SYSTEM STATUS");
        ImGui::TextWrapped("%s", launcher_.status_message().c_str());
        ImGui::Separator();

        if (show_settings_) {
            ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.70f, 1.00f), "Launcher Global Settings");
            ImGui::TextDisabled("Configure memory, runtime paths, and simulation options.");
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::InputText("Java Executable Override", settings_java_path_, sizeof(settings_java_path_));
            ImGui::TextDisabled("Leave blank to auto-detect system Java or download portable JRE.");

            ImGui::Spacing();
            if (ImGui::Button("Auto-Detect Installed Java JDKs/JREs")) {
                detected_javas_ = launcher_.detect_system_javas();
            }
            ImGui::SameLine();
            if (ImGui::Button("Download Portable Java JRE 21")) {
                launcher_.download_portable_java_async();
            }

            if (launcher_.is_installing()) {
                ImGui::Spacing();
                ImGui::Text("Active Task: %s", launcher_.install_stage().c_str());
                ImGui::ProgressBar(launcher_.install_progress(), ImVec2(-1, 28));
            }
            if (!detected_javas_.empty()) {
                ImGui::Text("Select a detected Java path:");
                for (const auto& j : detected_javas_) {
                    std::string label = j.first + " (" + j.second + ")";
                    if (ImGui::Button(label.c_str())) {
                        std::snprintf(settings_java_path_, sizeof(settings_java_path_), "%s", j.second.c_str());
                    }
                }
            } else {
                ImGui::TextDisabled("(Click Auto-Detect to scan your system for Java runtimes)");
            }

            ImGui::Spacing();
            ImGui::SliderInt("Max Allocated Memory (MB)", &settings_memory_mb_, 1024, 16384, "%d MB");
            ImGui::TextDisabled("Slide to allocate memory for the game client (recommended: 4096MB).");

            ImGui::Spacing();
            ImGui::InputText("Custom JVM Arguments", settings_jvm_args_, sizeof(settings_jvm_args_));
            ImGui::TextDisabled("Pass custom flags to the JVM (e.g. -XX:+UseG1GC).");

            ImGui::Spacing();
            ImGui::Checkbox("Always run simulation (mock launch)", &settings_force_mock_);
            ImGui::TextDisabled("If enabled, bypass Java execution and run the Mock client screen.");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Save Settings", ImVec2(160, 45))) {
                LauncherSettings s;
                s.java_path = settings_java_path_;
                s.allocated_memory_mb = settings_memory_mb_;
                s.jvm_args = settings_jvm_args_;
                s.force_mock = settings_force_mock_;
                launcher_.save_settings(s);
                show_settings_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 45))) {
                show_settings_ = false;
            }

        } else if (show_creation_) {
            // New Instance Screen
            ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.70f, 1.00f), "Create New Minecraft Instance");
            ImGui::Spacing();
            ImGui::InputText("Instance Name", new_instance_name_, sizeof(new_instance_name_));

            const std::vector<VersionEntry> versions = launcher_.versions();
            std::vector<const char*> version_names;
            for (const auto& v : versions) {
                version_names.push_back(v.id.c_str());
            }

            if (!version_names.empty()) {
                ImGui::Combo("Minecraft Version", &new_instance_version_idx_, version_names.data(), version_names.size());
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Warning: No versions available in catalog.");
            }

            ImGui::Spacing();
            ImGui::Text("Compatible Modloaders:");

            if (!versions.empty() && new_instance_version_idx_ >= 0 && new_instance_version_idx_ < static_cast<int>(versions.size())) {
                const std::string selected_ver_id = versions[new_instance_version_idx_].id;
                const std::string selected_ver_type = versions[new_instance_version_idx_].type;

                bool fabric_ok = is_loader_compatible(selected_ver_id, selected_ver_type, "fabric");
                bool forge_ok = is_loader_compatible(selected_ver_id, selected_ver_type, "forge");

                if (ImGui::RadioButton("Vanilla", new_instance_loader_idx_ == 0)) {
                    new_instance_loader_idx_ = 0;
                }
                
                ImGui::SameLine();
                if (!fabric_ok) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::RadioButton("Fabric", new_instance_loader_idx_ == 1)) {
                    new_instance_loader_idx_ = 1;
                }
                if (!fabric_ok) {
                    ImGui::EndDisabled();
                    if (new_instance_loader_idx_ == 1) {
                        new_instance_loader_idx_ = 0;
                    }
                }
                
                ImGui::SameLine();
                if (!forge_ok) {
                    ImGui::BeginDisabled();
                }
                if (ImGui::RadioButton("Forge", new_instance_loader_idx_ == 2)) {
                    new_instance_loader_idx_ = 2;
                }
                if (!forge_ok) {
                    ImGui::EndDisabled();
                    if (new_instance_loader_idx_ == 2) {
                        new_instance_loader_idx_ = 0;
                    }
                }

                if (!fabric_ok || !forge_ok) {
                    ImGui::TextDisabled("Note: Greyed out modloaders are not compatible with Minecraft %s.", selected_ver_id.c_str());
                }
            } else {
                if (ImGui::RadioButton("Vanilla", new_instance_loader_idx_ == 0)) {
                    new_instance_loader_idx_ = 0;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Create Instance", ImVec2(180, 45))) {
                std::string name(new_instance_name_);
                if (!name.empty() && !version_names.empty()) {
                    std::string loader_str = (new_instance_loader_idx_ == 1) ? "fabric" : (new_instance_loader_idx_ == 2 ? "forge" : "vanilla");
                    launcher_.create_instance(name, versions[new_instance_version_idx_].id, loader_str);
                    launcher_.reload_instances();
                    show_creation_ = false;
                    
                    // Select new instance
                    const auto updated = launcher_.instances();
                    for (std::size_t i = 0; i < updated.size(); ++i) {
                        if (updated[i].name == name) {
                            selected_instance_idx_ = static_cast<int>(i);
                            break;
                        }
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 45))) {
                show_creation_ = false;
                const auto insts = launcher_.instances();
                if (!insts.empty() && selected_instance_idx_ == -1) {
                    selected_instance_idx_ = 0;
                }
            }
        } else if (show_modrinth_ && selected_instance_idx_ >= 0 && selected_instance_idx_ < static_cast<int>(instances.size())) {
            const auto& instance = instances[selected_instance_idx_];
            ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.70f, 1.00f), "Install Addons for %s", instance.name.c_str());
            ImGui::TextDisabled("Minecraft %s | Search and download mods, resource packs, or shaderpacks directly from Modrinth.", instance.version_id.c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Combo("Addon Category", &modrinth_category_idx_, categories, 3)) {
                modrinth_search_[0] = '\0';
                selected_modrinth_project_idx_ = -1;
                checked_project_ids_.clear();
                
                std::string t = types[modrinth_category_idx_];
                std::string v = instance.version_id;
                modrinth_status_ = "Loading popular...";
                
                std::thread([this, t, v]() {
                    auto res = modrinth::search("", t, v);
                    modrinth_results_ = res;
                    modrinth_status_ = "Loaded popular addons.";
                }).detach();
            }
            
            ImGui::InputText("Search Addons", modrinth_search_, sizeof(modrinth_search_));
            ImGui::SameLine();
            if (ImGui::Button("Search Modrinth") || (modrinth_results_.empty() && !download_in_progress_["initial_load"])) {
                selected_modrinth_project_idx_ = -1;
                checked_project_ids_.clear();
                
                std::string q = modrinth_search_;
                std::string t = types[modrinth_category_idx_];
                std::string v = instance.version_id;
                
                modrinth_status_ = q.empty() ? "Loading popular..." : "Searching...";
                download_in_progress_["initial_load"] = true;
                
                std::thread([this, q, t, v]() {
                    auto res = modrinth::search(q, t, v);
                    modrinth_results_ = res;
                    modrinth_status_ = q.empty() ? "Loaded popular addons." : "Found " + std::to_string(res.size()) + " matches.";
                    download_in_progress_["initial_load"] = false;
                }).detach();
            }

            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", modrinth_status_.c_str());
            ImGui::Spacing();

            ImGui::BeginChild("ModrinthResults", ImVec2(380.0f, -60.0f), true);
            
            if (!checked_project_ids_.empty()) {
                std::string bulk_lbl = "Install Checked Addons (" + std::to_string(checked_project_ids_.size()) + ")";
                if (ImGui::Button(bulk_lbl.c_str(), ImVec2(-1, 35))) {
                    modrinth_status_ = "Installing checked addons in background...";
                    
                    std::vector<std::string> pids = checked_project_ids_;
                    std::string ver_id = instance.version_id;
                    std::string cat_type = types[modrinth_category_idx_];
                    std::string inst_name = instance.name;
                    
                    std::thread([this, pids, ver_id, cat_type, inst_name]() {
                        std::vector<ModrinthVersion> to_install;
                        for (const auto& pid : pids) {
                            auto vers = modrinth::get_versions(pid, ver_id);
                            if (!vers.empty()) {
                                to_install.push_back(vers.front());
                            }
                        }
                        if (!to_install.empty()) {
                            launcher_.install_modrinth_addons_async(to_install, cat_type, inst_name);
                        }
                    }).detach();
                    
                    checked_project_ids_.clear();
                }
                ImGui::Separator();
            }

            for (std::size_t i = 0; i < modrinth_results_.size(); ++i) {
                const auto& proj = modrinth_results_[i];
                
                ImTextureID img_id = 0;
                if (!proj.icon_url.empty()) {
                    if (project_textures_.find(proj.id) != project_textures_.end()) {
                        if (project_textures_[proj.id] != nullptr) {
                            img_id = (ImTextureID)(uintptr_t)project_textures_[proj.id];
                        }
                    } else {
                        std::filesystem::path app_dir = std::filesystem::path(std::getenv("HOME")) / "Library" / "Application Support" / "HLauncher";
                        std::filesystem::path cache_dir = app_dir / "cache" / "icons";
                        
                        std::string ext = ".png";
                        if (proj.icon_url.find(".jpg") != std::string::npos || proj.icon_url.find(".jpeg") != std::string::npos) {
                            ext = ".jpg";
                        }
                        std::filesystem::path cached_path = cache_dir / (proj.id + ext);
                        if (std::filesystem::exists(cached_path)) {
                            SDL_Texture* tex = load_texture_from_file(renderer, cached_path);
                            if (tex) {
                                project_textures_[proj.id] = tex;
                                img_id = (ImTextureID)(uintptr_t)tex;
                            } else {
                                project_textures_[proj.id] = nullptr;
                            }
                        } else if (!download_in_progress_[proj.id]) {
                            download_in_progress_[proj.id] = true;
                            std::string url = proj.icon_url;
                            std::thread([cached_path, url, proj_id = proj.id, this]() {
                                std::filesystem::create_directories(cached_path.parent_path());
                                std::string cmd = "curl -s -L -f -o \"" + cached_path.string() + "\" \"" + url + "\"";
                                std::system(cmd.c_str());
                                download_in_progress_[proj_id] = false;
                            }).detach();
                        }
                    }
                }
                
                bool is_checked = std::find(checked_project_ids_.begin(), checked_project_ids_.end(), proj.id) != checked_project_ids_.end();
                if (ImGui::Checkbox(("##check_" + proj.id).c_str(), &is_checked)) {
                    if (is_checked) {
                        checked_project_ids_.push_back(proj.id);
                    } else {
                        checked_project_ids_.erase(std::remove(checked_project_ids_.begin(), checked_project_ids_.end(), proj.id), checked_project_ids_.end());
                    }
                }
                
                ImGui::SameLine();
                
                ImVec2 pos = ImGui::GetCursorScreenPos();
                if (img_id != 0) {
                    ImGui::Image((void*)(uintptr_t)img_id, ImVec2(32, 32));
                } else {
                    ImGui::GetWindowDrawList()->AddRectFilled(
                        pos,
                        ImVec2(pos.x + 32, pos.y + 32),
                        IM_COL32(40, 48, 64, 255),
                        4.0f
                    );
                    ImGui::Dummy(ImVec2(32, 32));
                }
                
                ImGui::SameLine();
                
                ImGui::BeginGroup();
                bool is_sel = (selected_modrinth_project_idx_ == static_cast<int>(i));
                std::string sel_id = "##sel_" + proj.id;
                if (ImGui::Selectable((proj.title + sel_id).c_str(), is_sel, ImGuiSelectableFlags_None, ImVec2(240, 20))) {
                    selected_modrinth_project_idx_ = static_cast<int>(i);
                    selected_modrinth_version_idx_ = -1;
                    modrinth_status_ = "Loading files for " + proj.title + "...";
                    modrinth_versions_ = modrinth::get_versions(proj.id, instance.version_id);
                    if (modrinth_versions_.empty()) {
                        modrinth_status_ = "No compatible files found for Minecraft " + instance.version_id;
                    } else {
                        modrinth_status_ = "Found " + std::to_string(modrinth_versions_.size()) + " compatible versions.";
                    }
                }
                ImGui::TextDisabled("by %s", proj.author.c_str());
                ImGui::EndGroup();
                
                ImGui::Separator();
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("ModrinthProjectDetails", ImVec2(0.0f, -60.0f), true);
            if (selected_modrinth_project_idx_ >= 0 && selected_modrinth_project_idx_ < static_cast<int>(modrinth_results_.size())) {
                const auto& proj = modrinth_results_[selected_modrinth_project_idx_];
                ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.70f, 1.00f), "%s", proj.title.c_str());
                ImGui::TextDisabled("Slug: %s | Author: %s", proj.slug.c_str(), proj.author.c_str());
                ImGui::TextWrapped("%s", proj.description.c_str());
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                ImGui::Text("Available Files:");
                ImGui::BeginChild("VersionFiles", ImVec2(0.0f, 0.0f), false);
                for (std::size_t j = 0; j < modrinth_versions_.size(); ++j) {
                    const auto& ver = modrinth_versions_[j];
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.6f, 1.0f), "%s", ver.name.c_str());
                    ImGui::TextDisabled("File: %s | Version: %s", ver.filename.c_str(), ver.version_number.c_str());
                    
                    std::string btn_lbl = "Download & Install##" + ver.id;
                    if (ImGui::Button(btn_lbl.c_str(), ImVec2(180, 28))) {
                        modrinth_status_ = "Downloading " + ver.filename + "...";
                        if (modrinth::install_version(ver, types[modrinth_category_idx_], instance.directory)) {
                            modrinth_status_ = "Successfully installed " + ver.filename + "!";
                        } else {
                            modrinth_status_ = "Error downloading " + ver.filename;
                        }
                    }
                    ImGui::Separator();
                }
                ImGui::EndChild();
            } else {
                ImGui::TextDisabled("Select a project to inspect available compatible versions.");
            }
            ImGui::EndChild();

            ImGui::Spacing();
            if (ImGui::Button("Back to Instance View", ImVec2(200, 45))) {
                show_modrinth_ = false;
            }

        } else if (selected_instance_idx_ >= 0 && selected_instance_idx_ < static_cast<int>(instances.size())) {
            // Selected Instance View
            const auto& instance = instances[selected_instance_idx_];
            
            ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.70f, 1.00f), "%s", instance.name.c_str());
            ImGui::TextDisabled("Version: %s | Modloader: %s", instance.version_id.c_str(), instance.loader.c_str());
            ImGui::TextDisabled("Directory: %s", instance.directory.string().c_str());
            ImGui::SameLine();
            if (ImGui::Button("Open Folder")) {
#if defined(_WIN32)
                std::string cmd = "explorer \"" + instance.directory.string() + "\"";
#elif defined(__APPLE__)
                std::string cmd = "open \"" + instance.directory.string() + "\"";
#else
                std::string cmd = "xdg-open \"" + instance.directory.string() + "\"";
#endif
                std::system(cmd.c_str());
            }
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            bool installed = launcher_.is_instance_installed(instance.name);
            bool installing = launcher_.is_installing();
            bool running_game = launcher_.is_game_running();

            if (installing) {
                ImGui::Text("Installing: %s", launcher_.install_stage().c_str());
                ImGui::ProgressBar(launcher_.install_progress(), ImVec2(-1, 35));
            } else if (running_game) {
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "Game runtime active.");
                ImGui::Spacing();
                if (ImGui::Button("Kill Game Process", ImVec2(240, 50))) {
                    launcher_.kill_game_process();
                }
            } else {
                if (!installed) {
                    ImGui::Text("Minecraft runtime resources must be installed for this instance.");
                    ImGui::Spacing();
                    if (ImGui::Button("Install Minecraft Assets & Libraries", ImVec2(320, 50))) {
                        launcher_.install_instance_async(instance.name);
                    }
                } else {
                    if (ImGui::Button("LAUNCH GAME", ImVec2(220, 55))) {
                        launcher_.launch_instance_async(instance.name, force_mock_);
                    }
                    ImGui::SameLine();
                    ImGui::Checkbox("Launch simulation (no Java required)", &force_mock_);

                    ImGui::Spacing();
                    if (ImGui::Button("GET MODS / ADDONS (MODRINTH)", ImVec2(320, 45))) {
                        show_modrinth_ = true;
                        show_creation_ = false;
                        show_settings_ = false;
                        modrinth_search_[0] = '\0';
                        modrinth_results_.clear();
                        modrinth_versions_.clear();
                        checked_project_ids_.clear();
                        selected_modrinth_project_idx_ = -1;
                        selected_modrinth_version_idx_ = -1;
                        modrinth_status_ = "Loading popular addons...";
                        
                        std::thread([this, ver_id = instance.version_id]() {
                            const char* types[] = { "mod", "resourcepack", "shaderpack" };
                            auto res = modrinth::search("", types[modrinth_category_idx_], ver_id);
                            modrinth_results_ = res;
                            modrinth_status_ = "Loaded popular addons.";
                        }).detach();
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Console outputs
            ImGui::TextDisabled("GAME RUNTIME CONSOLE LOGS");
            ImGui::SameLine();
            ImGui::Checkbox("Auto-scroll", &auto_scroll_logs_);
            ImGui::SameLine();
            if (ImGui::Button("Clear Logs")) {
                launcher_.clear_game_logs();
            }
            ImGui::SameLine();
            if (ImGui::Button("Copy Logs")) {
                std::string all_logs;
                const auto logs = launcher_.game_logs();
                for (const auto& log : logs) {
                    all_logs += log + "\n";
                }
                ImGui::SetClipboardText(all_logs.c_str());
            }

            ImGui::BeginChild("LogConsole", ImVec2(-1, -60.0f), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 3));
            
            const auto logs = launcher_.game_logs();
            for (const auto& log : logs) {
                if (log.find("[Launcher/ERROR]") != std::string::npos || log.find("/ERROR]") != std::string::npos) {
                    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%s", log.c_str());
                } else if (log.find("[Launcher/WARNING]") != std::string::npos || log.find("/WARNING]") != std::string::npos) {
                    ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.20f, 1.0f), "%s", log.c_str());
                } else if (log.find("[Launcher]") != std::string::npos) {
                    ImGui::TextColored(ImVec4(0.00f, 0.85f, 0.70f, 1.00f), "%s", log.c_str());
                } else {
                    ImGui::TextUnformatted(log.c_str());
                }
            }

            if (auto_scroll_logs_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 15.0f) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::PopStyleVar();
            ImGui::EndChild();

            ImGui::Spacing();
            if (ImGui::Button("Delete Instance", ImVec2(150, 30))) {
                launcher_.delete_instance(instance.name);
                selected_instance_idx_ = -1;
                show_creation_ = false;
                const auto insts = launcher_.instances();
                if (!insts.empty()) {
                    selected_instance_idx_ = 0;
                }
            }

        } else {
            ImGui::TextDisabled("Select or create a Minecraft instance to start.");
        }

        ImGui::EndChild(); // End Dashboard

        ImGui::End();

        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, 18, 20, 24, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
