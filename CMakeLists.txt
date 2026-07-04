cmake_minimum_required(VERSION 3.20)

project(HLauncher VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}/bin/${config})
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}/lib/${config})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${config_upper} ${CMAKE_BINARY_DIR}/lib/${config})
endforeach()

add_executable(HLauncher
    src/main.cpp
    src/app/launcher.cpp
    src/app/account_store.cpp
    src/app/instance_manager.cpp
    src/app/version_catalog.cpp
    src/app/downloader.cpp
    src/app/modrinth.cpp
    src/auth/oauth.cpp
    src/auth/pkce.cpp
    src/auth/redirect.cpp
    src/ipc/single_instance.cpp
    src/platform/open_url.cpp
    src/ui/launcher_window.cpp
)

target_include_directories(HLauncher PRIVATE src)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.12
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)

FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL2 imgui nlohmann_json)

target_include_directories(HLauncher PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_sources(HLauncher PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
)

if (WIN32)
    target_link_libraries(HLauncher PRIVATE SDL2::SDL2 SDL2::SDL2main nlohmann_json::nlohmann_json)
else()
    target_link_libraries(HLauncher PRIVATE SDL2::SDL2 nlohmann_json::nlohmann_json)
endif()

if (APPLE)
    target_sources(HLauncher PRIVATE src/platform/macos_url.mm)

    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in
        ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
        @ONLY
    )
    set_target_properties(HLauncher PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "HLauncher"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.guille.hlauncher"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    )

    target_link_libraries(HLauncher PRIVATE "-framework AppKit")
endif()

target_compile_options(HLauncher PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

if (WIN32)
    target_link_libraries(HLauncher PRIVATE Ws2_32 Shell32)
endif()

install(TARGETS HLauncher
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION bin
)
