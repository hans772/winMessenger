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

#include "client/client.hpp"
#include "server/server.hpp"

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

void enable_ansi() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode;
    GetConsoleMode(hConsole, &mode);
    SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

int main() {
    glfwInit();
    GLFWwindow* window = glfwCreateWindow(800, 600, "WinMessenger", NULL, NULL);
    GL_init(window);

    SceneManager::get().push(std::make_unique<MenuScreen>());

    while (!glfwWindowShouldClose(window) && !SceneManager::get().should_quit) {
        SceneManager::get().top().poll_events();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        SceneManager::get().top().on_update();

        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    enable_ansi();

    // launch terminal app after GL is fully torn down
    auto& sm = SceneManager::get();
    std::string port_str = std::to_string(sm.selected_port);

    if (sm.startup_selection == STARTUP_ITEM::SERVER) {
        ChatServer server;
        server.start_server(port_str.c_str(), sm.max_clients);

        std::string cmd;
        while (std::getline(std::cin, cmd)) {
            if (cmd == "quit") break;
        }
        server.stop_server();

    }
    else {
        ChatClient client;
        client.start_client(sm.selected_ip.c_str(), port_str.c_str());
        client.input_loop();
        client.stop_client();
    }

    return 0;


}