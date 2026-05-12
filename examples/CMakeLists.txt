add_executable(phase0_demo phase0_demo.cpp)
target_link_libraries(phase0_demo PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_shaders(phase0_demo)

add_executable(phase1_demo phase1_demo.cpp)
target_link_libraries(phase1_demo PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_shaders(phase1_demo)

add_executable(phase2_demo phase2_demo.cpp)
target_link_libraries(phase2_demo PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_shaders(phase2_demo)

add_executable(phase2b_demo phase2b_demo.cpp)
target_link_libraries(phase2b_demo PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_shaders(phase2b_demo)

add_executable(phase3_demo phase3_demo.cpp)
target_link_libraries(phase3_demo PRIVATE sdl_painter spdlog::spdlog_header_only)
sdlpainter_copy_shaders(phase3_demo)

add_executable(phase4_demo phase4_demo.cpp)
if(TARGET SDL3_ttf::SDL3_ttf-static)
    target_link_libraries(phase4_demo PRIVATE sdl_painter SDL3_ttf::SDL3_ttf-static spdlog::spdlog_header_only)
else()
    target_link_libraries(phase4_demo PRIVATE sdl_painter SDL3_ttf::SDL3_ttf spdlog::spdlog_header_only)
endif()
sdlpainter_copy_shaders(phase4_demo)

if(SDLPAINTER_WITH_VULKAN)
    add_executable(phase5e_vulkan_text phase5e_vulkan_text.cpp)
    if(TARGET SDL3_ttf::SDL3_ttf-static)
        target_link_libraries(phase5e_vulkan_text
            PRIVATE sdl_painter SDL3_ttf::SDL3_ttf-static spdlog::spdlog_header_only)
    else()
        target_link_libraries(phase5e_vulkan_text
            PRIVATE sdl_painter SDL3_ttf::SDL3_ttf spdlog::spdlog_header_only)
    endif()
    sdlpainter_copy_vulkan_shaders(phase5e_vulkan_text)
endif()

if(SDLPAINTER_WITH_VULKAN)
    add_executable(phase5a_vulkan_clear phase5a_vulkan_clear.cpp)
    target_link_libraries(phase5a_vulkan_clear
        PRIVATE sdl_painter spdlog::spdlog_header_only)
    sdlpainter_copy_vulkan_shaders(phase5a_vulkan_clear)

    add_executable(phase5b_vulkan_triangles phase5b_vulkan_triangles.cpp)
    target_link_libraries(phase5b_vulkan_triangles
        PRIVATE sdl_painter spdlog::spdlog_header_only)
    sdlpainter_copy_vulkan_shaders(phase5b_vulkan_triangles)

    add_executable(phase5c_vulkan_textured phase5c_vulkan_textured.cpp)
    target_link_libraries(phase5c_vulkan_textured
        PRIVATE sdl_painter spdlog::spdlog_header_only)
    sdlpainter_copy_vulkan_shaders(phase5c_vulkan_textured)

    add_executable(phase5d_vulkan_demo phase5d_vulkan_demo.cpp)
    target_link_libraries(phase5d_vulkan_demo
        PRIVATE sdl_painter spdlog::spdlog_header_only)
    sdlpainter_copy_vulkan_shaders(phase5d_vulkan_demo)
endif()
