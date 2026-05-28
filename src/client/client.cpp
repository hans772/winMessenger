#include "client/client.hpp"
#include "util/config.hpp"
#include "util/payloads.hpp"

#include <iostream>
#include <conio.h>

void ChatClient::clear_input_line() {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(hConsole, &info);

    COORD line_start = { 0, info.dwCursorPosition.Y };
    SetConsoleCursorPosition(hConsole, line_start);

    DWORD written;
    FillConsoleOutputCharacter(hConsole, ' ', info.dwSize.X, line_start, &written);

    SetConsoleCursorPosition(hConsole, line_start);
}

void ChatClient::restore_input() {
    clear_input_line();
    std::cout << "> " << current_input;
}

void ChatClient::on_connect_to_server() {}

void ChatClient::on_request_login(WMNet::ServerMessage msg) {
    awaiting_login.store(true);
}

void ChatClient::on_acknowledge_login(WMNet::ServerMessage msg) {
    awaiting_login.store(false);
}

void ChatClient::on_client_message(WMNet::ServerMessage msg) {
    std::lock_guard<std::mutex> lock(console_mtx);
    ClientMessage cmsg = WMNet::DeSerializer<ClientMessage>::deserialize(msg.data);
    
    clear_input_line();

    std::cout << cmsg.client_name << ": " << cmsg.message << std::endl;

    restore_input();
}

void ChatClient::on_user_join(WMNet::ServerMessage msg) {
    std::lock_guard<std::mutex> lock(console_mtx);
    clear_input_line();

    std::cout << WMNet::DeSerializer<std::string>::deserialize(msg.data) << " just joined the room!" << std::endl;

    restore_input();
}

void ChatClient::on_user_leave(WMNet::ServerMessage msg) {
    std::lock_guard<std::mutex> lock(console_mtx);
    clear_input_line();

    std::cout << WMNet::DeSerializer<std::string>::deserialize(msg.data) << " just left the room." << std::endl;

    restore_input();
}

void ChatClient::handle_input(const std::string& input) {
    if (awaiting_login.load()) {
        send_message(WMNet::Message<std::string>{.data = input, .id = ID_ClientLogin});
        return;
    }

    if (input[0] != '/') {
        send_message(WMNet::Message<std::string>{.data = input, .id = ID_ClientMessage});
    }
    else {
        // handle / commands
    }
}

void ChatClient::input_loop() {
    while (is_connected()) {
        char c = _getch();

        std::unique_lock lock(console_mtx);

        if (c == '\r') {
            std::string cmd = current_input;
            current_input.clear();
            clear_input_line();
            lock.unlock();
            handle_input(cmd);
        }
        else if (c == '\b') {
            if (!current_input.empty()) {
                current_input.pop_back();
                restore_input();
            }
        }
        else {
            current_input.push_back(c);
            restore_input();
        }
    }
}

ChatClient::ChatClient() : WMNet::Client() {
    register_handler(ID_RequestLogin, std::bind(&ChatClient::on_request_login, this, std::placeholders::_1));
    register_handler(ID_AcknowledgeLogin, std::bind(&ChatClient::on_acknowledge_login, this, std::placeholders::_1));
    register_handler(ID_ClientJoined, std::bind(&ChatClient::on_user_join, this, std::placeholders::_1));
    register_handler(ID_ClientMessage, std::bind(&ChatClient::on_client_message, this, std::placeholders::_1));
    register_handler(ID_ClientLeave, std::bind(&ChatClient::on_user_leave, this, std::placeholders::_1));
}