cmake_minimum_required(VERSION 3.15)
project(TestVulkan VERSION 0.0.1 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Vulkan REQUIRED)

set(GLFW_FOUND TRUE)
set(GLFW_INCLUDE_DIR "Thirdparty/glfw-prebuild/include")
set(GLFW_LIBRARY "Thirdparty/glfw-prebuild/lib-vc2022/glfw3.lib")

# Ref: https://thatonegamedev.com/cpp/cmake/how-to-compile-shaders-with-cmake/
function(add_shaders TARGET_NAME BUILD_TARGET_DIR)
    set(SHADER_SOURCE_FILES ${ARGN}) # the rest of arguments to this function will be assigned as shader source files
    
    # Validate that source files have been passed
    list(LENGTH SHADER_SOURCE_FILES FILE_COUNT)
    if(FILE_COUNT EQUAL 0)
        message(FATAL_ERROR "Cannot create a shaders target without any source files")
    endif()

    set(SHADER_COMMANDS)
    set(SHADER_PRODUCTS)

    foreach(SHADER_SOURCE IN LISTS SHADER_SOURCE_FILES)
        get_filename_component(SHADER_SOURCE "${SHADER_SOURCE}" REALPATH BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
        get_filename_component(BUILD_TARGET_DIR "${BUILD_TARGET_DIR}" REALPATH BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
        
        # Build command
        list(APPEND SHADER_COMMANDS COMMAND)
        list(APPEND SHADER_COMMANDS Vulkan::glslc)
        list(APPEND SHADER_COMMANDS "${SHADER_SOURCE}")
        list(APPEND SHADER_COMMANDS "-o")
        list(APPEND SHADER_COMMANDS "${BUILD_TARGET_DIR}/${SHADER_NAME}.spv")

        # Add product
        list(APPEND SHADER_PRODUCTS "${BUILD_TARGET_DIR}/${SHADER_NAME}.spv")

    endforeach()

    add_custom_target(${TARGET_NAME} ALL
        ${SHADER_COMMANDS}
        COMMENT "Compiling Shaders [${TARGET_NAME}]"
        SOURCES ${SHADER_SOURCE_FILES}
        BYPRODUCTS ${SHADER_PRODUCTS}
    )
endfunction()


add_executable(main "main.cpp")
target_include_directories(main PRIVATE ${Vulkan_INCLUDE_DIR} ${GLFW_INCLUDE_DIR})
target_link_libraries(main ${Vulkan_LIBRARY} ${GLFW_LIBRARY})

add_shaders(main_shader "shaders" "shaders/shader.vert" "shaders/shader.frag")
add_dependencies(main main_shader)

if (MSVC)
    target_compile_options(main PRIVATE /NODEFAULTLIB:library)
endif(MSVC)
