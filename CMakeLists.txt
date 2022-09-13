    cmake_minimum_required(VERSION 3.16)
    project(PathTracer)

    set(CMAKE_CXX_STANDARD 17)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc")
    find_package(Vulkan REQUIRED)

    add_subdirectory(lib/glfw)
    SET(GLM_TEST_ENABLE OFF CACHE BOOL "GLM Build unit tests")
    add_subdirectory(lib/glm    EXCLUDE_FROM_ALL)

    add_executable(PathTracer main.cpp PathTracerApp.cpp VulkanUtils.cpp Camera.cpp Camera.hpp)

    target_link_libraries(PathTracer glfw ${GLFW_LIBRARIES} glm::glm Vulkan::Vulkan)