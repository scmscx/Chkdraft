#pragma once
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <string>
#include <atomic>

namespace glfw
{
    // Global flag to track if GL context is valid
    // OpenGL object destructors should check this before calling GL functions
    inline std::atomic<bool> g_gl_context_valid{false};

    inline bool isGlContextValid() {
        return g_gl_context_valid.load(std::memory_order_acquire);
    }

    struct Context
    {
        Context()
        {
            glfwSetErrorCallback([](int errorCode, const char* description) {
                throw std::runtime_error("A glfw error occured: [" + std::to_string(errorCode) + "] " + description);
            });

            if ( glfwInit() == GLFW_FALSE )
                throw std::runtime_error("Failed to initialize glfw!");

            g_gl_context_valid.store(true, std::memory_order_release);
        }

        ~Context()
        {
            g_gl_context_valid.store(false, std::memory_order_release);
            glfwTerminate();
        }
    };
}