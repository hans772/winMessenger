#include "client/client.hpp"
#include "util/config.hpp"
#include "util/payloads.hpp"

#include <iostream>
#include <conio.h>
#include <sstream>

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
    std::cout << Color::RESET << "> " << current_input;
}

void ChatClient::on_connect_to_server() {
    std::cout << Color::YELLOW << "Connected to server :)" << std::endl;
}

void ChatClient::on_disconnect() {
    std::cout << Color::RED << "Disconnected from server :( " << std::endl;
}

void ChatClient::on_request_login(WMNet::ServerMessage msg) {
    std::cout << Color::YELLOW << "Enter Login Username: " << std::endl;
    awaiting_login.store(true);
}

void ChatClient::on_acknowledge_login(WMNet::ServerMessage msg) {
    std::cout << Color::GREEN << "Successfully logged in as: " << name << std::endl;
    awaiting_login.store(false);
}

void ChatClient::on_client_message(WMNet::ServerMessage msg) {
    std::lock_guard<std::mutex> lock(console_mtx);
    ClientString cmsg = WMNet::DeSerializer<ClientString>::deserialize(msg.data);
    
    clear_input_line();

    std::cout << Color::CYAN << cmsg.client_name << Color::RESET << ": " << cmsg.str << std::endl;

    restore_input();
}

void ChatClient::on_server_message(WMNet::ServerMessage msg) {
    std::lock_guard<std::mutex> lock(console_mtx);
    clear_input_line();

    std::cout << Color::YELLOW << "[SERVER]: " << WMNet::DeSerializer<std::string>::deserialize(msg.data) << std::endl;

    restore_input();
}

void ChatClient::handle_input(const std::string& input) {
    if (awaiting_login.load()) {
        name = input;
        send_message(WMNet::Message<std::string>{.data = input, .id = ID_ClientLogin});
        return;
    }

    std::unique_lock lock(console_mtx);

    if (input[0] != '/') {
        std::cout << Color::MAGENTA << "You: " << Color::RESET << input << std::endl;

        send_message(WMNet::Message<std::string>{.data = input, .id = ID_ClientMessage});
    }
    else {
        std::string command;
        std::stringstream input_stream(input);
        input_stream >> command;
        std::map<std::string, std::string> kwargs;
        std::vector<std::string> args;
        std::string arg;
        while (input_stream >> arg) {
            if (arg.starts_with("--")) {
                int eq = arg.find_first_of('=');
                if (eq == std::string::npos || eq == arg.size() - 1) {
                    std::cout << Color::RED << "No value for keyword argument: " << arg << std::endl;
                    return;
                }
                kwargs[arg.substr(2, eq - 2)] = arg.substr(eq + 1, arg.size() - eq - 1);
                continue;
            }
            args.push_back(arg);
        }

        if (command == "/subroom") {
            if (args.empty()) {
                std::cout << Color::RED << "Invalid Arguments." << Color::RESET << std::endl;
                return;
            }

            if (args[0] == "join") {
                std::map<std::string, std::string>::iterator passed;
                if ((passed = kwargs.find("id")) != kwargs.end()) {
                    send_message(WMNet::Message<std::uint32_t>{.data = static_cast<uint32_t>(std::stoi(passed->second)), .id = ID_JoinSubroom});
                    return;
                }
                if ((passed = kwargs.find("name")) != kwargs.end()) {
                    send_message(WMNet::Message<std::string>{.data = passed->second, .id = ID_JoinSubroom});
                    return;
                }
                std::cout << Color::RED << "Missing Keyword Arguments 'id' or 'name' for " << Color::RESET << "/ subroom join" << std::endl;
                return;
            }

            if (args[0] == "create") {
                if (args.size() < 2) {
                    std::cout << Color::RED << "Invalid Arguments." << Color::RESET << std::endl;
                    return;
                }
                send_message(WMNet::Message<std::string>{.data = args[1], .id = ID_CreateSubroom});
                return;
            }

            if (args[0] == "parent") {
                send_message(WMNet::Message<WMNet::Empty>{.id = ID_BackToParent});
                return;
            }

            std::cout << Color::RED << "Invalid Arguments." << Color::RESET << std::endl;
            return;
        }
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
    register_handler(ID_ClientMessage, std::bind(&ChatClient::on_client_message, this, std::placeholders::_1));
    register_handler(ID_ServerMessage, std::bind(&ChatClient::on_server_message, this, std::placeholders::_1));
}