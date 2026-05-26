#include <WinSock2.h>
#include <WS2tcpip.h>
#include <string>

#include "scenes/scenes.hpp"
#include "scenes/scene_manager.hpp"

Window::Window(const char* name, ImGuiWindowFlags flags = 0) {
    ImGui::Begin(name, NULL, flags);
}

Window::Window(const char* name, ImVec4 rect, ImGuiWindowFlags flags = 0) {
    ImGui::SetNextWindowPos(ImVec2(rect.x, rect.y));
    ImGui::SetNextWindowSize(ImVec2(rect.z, rect.w));
    ImGui::Begin(name, NULL, flags);
}
Window::Window(const char* name, ImVec2 pos, ImGuiWindowFlags flags = 0) {
    ImGui::SetNextWindowPos(pos);
    ImGui::Begin(name, NULL, flags);
}

Window::~Window() {
    ImGui::End();
}

MenuScreen::MenuScreen(): startup((int)STARTUP_ITEM::SERVER), port(5555), server_max_clients(2), client_ip("localhost"), start_button_lbl("Unknown") {}

void MenuScreen::start_app() {
    if (startup == (int)STARTUP_ITEM::SERVER) {

    }
}

void MenuScreen::on_update() {
    ImGui::NewFrame();

    {
        Window select_startup("Select Startup Item", ImVec4(0.f, 0.f, 800.f, 100.f), ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::RadioButton("Server", &startup, (int)STARTUP_ITEM::SERVER);
        ImGui::RadioButton("Client", &startup, (int)STARTUP_ITEM::CLIENT);
    };

    if (startup == (int)STARTUP_ITEM::SERVER) {
        int inputs_offset_x = 200.f;
        start_button_lbl = "Start Server!";
        Window server_opt("Server Options", ImVec4(0.f, 105.f, 800.f, 100.f), ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        ImGui::Text("Enter Port: ");
        ImGui::SameLine(inputs_offset_x);
        ImGui::InputInt("##port", &port);
        ImGui::Text("Enter Maximum Clients: ");
        ImGui::SameLine(inputs_offset_x);
        ImGui::InputInt("##max_clients", &server_max_clients);
    }
    else {
        int inputs_offset_x = 200.f;
        start_button_lbl = "Start Client!";

        Window server_opt("Client Options", ImVec4(0.f, 105.f, 800.f, 100.f), ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("Enter Port: ");
        ImGui::SameLine(inputs_offset_x);
        ImGui::InputInt("##port", &port);
        ImGui::Text("Enter Server IP Address: ");
        ImGui::SameLine(inputs_offset_x);
        ImGui::InputTextWithHint("##ip_address", "192.168.0.1", &client_ip);
    }

    {
        Window server_opt("_", ImVec4(0.f, 210.f, 800.f, 50.f), ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::Button(start_button_lbl.c_str(), ImVec2(800.f, 40.f));
        ImGui::SetItemTooltip(start_button_lbl.c_str());
    };
    
    
    ImGui::Render();
}

void MenuScreen::on_startup() {}

void MenuScreen::on_pause() {}

void MenuScreen::on_resume() {}

void MenuScreen::on_exit() {}

void MenuScreen::poll_events() {
    glfwPollEvents();
}

MenuScreen::~MenuScreen() {
	
}
