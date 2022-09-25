#include "PathTracerApp.hpp"

#include <iostream>

int main() {
    auto& app = PathTracerApp::instance();
    app.initSettings("PathTracer", 1280, 720, "cornell_box", 16);
    app.run();
}