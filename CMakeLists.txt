cmake_minimum_required(VERSION 3.24)
project(VanguardEngine VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VANGUARD_BUILD_TESTS "Build GoogleTest unit and determinism suites" ON)
option(VANGUARD_ENABLE_SANITIZERS "Enable ASan and UBSan" OFF)

include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.0
    GIT_SHALLOW TRUE
)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG v2.0.8
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR cmake
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.5-docking
    GIT_SHALLOW TRUE
)
FetchContent_Declare(
    implot
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG v0.16
    GIT_SHALLOW TRUE
)

if(VANGUARD_BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
        GIT_SHALLOW TRUE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
endif()

FetchContent_MakeAvailable(SDL3 glad glm json imgui implot)
glad_add_library(glad_gl_core_45 REPRODUCIBLE API gl:core=4.5)

add_library(imgui_implot STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${implot_SOURCE_DIR}/implot.cpp
    ${implot_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(imgui_implot PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${implot_SOURCE_DIR}
)
target_link_libraries(imgui_implot PUBLIC SDL3::SDL3 glad_gl_core_45)

add_library(VanguardCore STATIC
    engine/render/BatchRenderer.cpp
)
target_include_directories(VanguardCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VanguardCore PUBLIC
    SDL3::SDL3
    glad_gl_core_45
    glm::glm
    nlohmann_json::nlohmann_json
    imgui_implot
)
target_compile_definitions(VanguardCore PUBLIC ASSETS_DIR="assets")

add_library(VanguardCombatLib STATIC
    game/combat/CombatWorld.cpp
)
target_include_directories(VanguardCombatLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(VanguardCombatLib PUBLIC VanguardCore)

add_executable(VanguardGame game/main.cpp)
target_link_libraries(VanguardGame PRIVATE VanguardCombatLib)

if(VANGUARD_BUILD_TESTS)
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_executable(VanguardTests tests/VanguardTests.cpp)
    target_link_libraries(VanguardTests PRIVATE VanguardCombatLib GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(VanguardTests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endif()

set(VANGUARD_TARGETS VanguardCore VanguardCombatLib VanguardGame)
if(VANGUARD_BUILD_TESTS)
    list(APPEND VANGUARD_TARGETS VanguardTests)
endif()
foreach(tgt ${VANGUARD_TARGETS})
    if(MSVC)
        target_compile_options(${tgt} PRIVATE /W4)
    else()
        target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
        if(VANGUARD_ENABLE_SANITIZERS)
            target_compile_options(${tgt} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${tgt} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endforeach()
