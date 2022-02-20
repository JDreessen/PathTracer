    cmake_minimum_required(VERSION 3.16)
    project(PathTracer)

    set(CMAKE_CXX_STANDARD 17)
    find_package(Vulkan REQUIRED)

    add_subdirectory(lib/glfw)
    SET(GLM_TEST_ENABLE OFF CACHE BOOL "GLM Build unit tests")
    add_subdirectory(lib/glm    EXCLUDE_FROM_ALL)

    add_executable(PathTracer main.cpp PathTracerApp.cpp VulkanUtils.cpp)

    target_link_libraries(PathTracer glfw ${GLFW_LIBRARIES} Vulkan::Vulkan)