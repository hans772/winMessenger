#include <GLFW/glfw3.h>
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "misc/cpp/imgui_stdlib.h"

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>
#include <thread>
#include  <random>
#include  <iterator>

#include "scenes/scene_manager.hpp"
#include "scenes/scenes.hpp"

#include "net/server.hpp"
#include "net/client.hpp"
#include "net/message.hpp"
#include "net/serialize.hpp"
#include "net/deserialize.hpp"

void GL_init(GLFWwindow* window) {
    glfwMakeContextCurrent(window);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(800, 600, "WinMessenger", NULL, NULL);
    GL_init(window);

    SceneManager::get().push(std::make_unique<MenuScreen>());

    while (!glfwWindowShouldClose(window)) {
        SceneManager::get().top().poll_events();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        SceneManager::get().top().on_update();
        
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui::DestroyContext();

    glfwTerminate();
}