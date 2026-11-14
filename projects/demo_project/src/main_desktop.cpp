// M0 smoke test: proves the host compiler, CMake, GLFW and OpenGL 3.3 context work.
// Replaced by the desktop platform layer + editor viewport in M2/M5.

#include <GLFW/glfw3.h>

#include <cstdio>

#include "e3d/core/log.hpp"

int main() {
    if (glfwInit() == GLFW_FALSE) {
        E3D_LOGE("glfwInit failed");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "3ds-engine (desktop)", nullptr, nullptr);
    if (window == nullptr) {
        E3D_LOGE("failed to create a GL 3.3 window");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    E3D_LOGI("OpenGL %s on %s", version != nullptr ? version : "?", renderer != nullptr ? renderer : "?");

    float t = 0.0f;
    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        t += 0.016f;
        glClearColor(0.04f + 0.02f * (0.5f + 0.5f * (t * 0.5f)), 0.05f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
