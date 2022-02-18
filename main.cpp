#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "lib/glm/glm/vec4.hpp"
#include "lib/glm/glm/mat4x4.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <iostream>

const uint32_t WIDTH = 1280;
const uint32_t HEIGHT = 720;
const std::string AppName = "PathTracer";
const std::string EngineName = "Vulkan.hpp";

class PathTracerApp {
public:
    // init glfw and vulkan
    void run() {
        initGLFW();
        initVulkan();
        mainLoop();
    }
    static PathTracerApp& instance() {
        static PathTracerApp _instance;
        return _instance;
    }
    // free glfw and vulkan resources
    ~PathTracerApp() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    PathTracerApp(const PathTracerApp&) = delete;
    PathTracerApp& operator=(const PathTracerApp&) = delete;

    void mainLoop() {
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();

            glfwSetKeyCallback(window,[](GLFWwindow* callbackWindow, int key, int scancode, int action, int mods) {
                if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(callbackWindow, 1);
            });
        }
    }
private:
    GLFWwindow* window{};

    vk::raii::Context context;
    vk::ApplicationInfo applicationInfo;
    vk::InstanceCreateInfo instanceCreateInfo;
    std::unique_ptr<vk::raii::Instance> vkInstance;

    PathTracerApp() : context(){

    }

    void initGLFW() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);
    }
    void initVulkan() {
        applicationInfo = vk::ApplicationInfo( AppName.c_str(), 1, EngineName.c_str(), 1, VK_API_VERSION_1_1 );
        instanceCreateInfo = vk::InstanceCreateInfo( {}, &applicationInfo );
        vkInstance = std::make_unique<vk::raii::Instance>(vk::raii::Instance( context, instanceCreateInfo ));
    }
};

int main() {
    auto& app = PathTracerApp::instance();
    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}