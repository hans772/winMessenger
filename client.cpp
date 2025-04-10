#include "client.hpp"
#include "json.hpp"
#include "message.hpp"
#include <iostream>
#include <sstream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <conio.h>

using namespace std;
std::mutex console_mutex;

Client::Client() {   }

int Client::check(int result, string oper) {
    // checking for WSAerrors
    if (result != 0) {
        std::cout << "Encountered an error in: `" << oper << "` with code: " << WSAGetLastError() << endl;;
        return 0;
    }
    return 1;
}

int Client::create_tcp_socket(const char* ip, const char* port) {

    /*
    This function sets up the socket for recieving connections from client.
        class variable hints is modified to determine the type of connection
        AF_INET means that connections of IPv4 are accepted
        SOCK_STREAM means that it allows for unbroken, ordered transmission of data with minimal loss.
        AI_PASSIVE allows for accepting connections.
    */

    struct addrinfo* result = NULL, * ptr = NULL;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // getaddrinfo replaces result with a linked list with available server sockets.

    if (!check(getaddrinfo(ip, port, &hints, &result), "GetADDRINFO")) {
        return 0;
    }
    _connect_socket = INVALID_SOCKET;

    ptr = result;

    // looping through available server sockets to find one to connect to.
    while (ptr != NULL) {
        _connect_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        // if create socket fails, go to next socket
        if (_connect_socket == INVALID_SOCKET) {
            ptr = ptr->ai_next;
            if (ptr == NULL) {
                std::cout << "Unable to create socket." << endl;
                return 0;
            }
            continue;
        }

        // if connecting to server fails, go to next socket

        if (!check(connect(_connect_socket, ptr->ai_addr, (int)ptr->ai_addrlen), "Server Connect")) {
            closesocket(_connect_socket);
            _connect_socket = INVALID_SOCKET;

            ptr = ptr->ai_next;
            if (ptr == NULL) {
                std::cout << "Unable to connect to available server" << endl;
                return 0;
            }
            continue;
        }

        std::cout << "Connected to server on port: " << port << endl;
        break;
    }

    // memory management

    freeaddrinfo(result);
    _connected = 1;

    return 1;
}

int ThreadedClient::start_chat() {
    string name;
    std::cout << "Enter your name: ";
    cin >> name;

    string room;
    std::cout << "Enter the Room you wish to join: ";
    cin >> room;

    std::cout << "-------------------------" << room << "-------------------------- - " << endl;
    // creating a join message to send to server as initial message

    Message msg = Message(CLIENT_JOIN);
    msg.set_body_string(name);
    msg.send_message(_connect_socket);
    msg.set_body_string(room);
    msg.send_message(_connect_socket);

    // creating a write thread, one can also create a listen_thread if they intend on doing other things on the main thread

    //thread listen(&ThreadedClient::_listen_thread, this, _connect_socket);
    thread write(&ThreadedClient::_write_thread, this, _connect_socket);

    // write runs in parallel
    write.detach();
    _listen_thread(_connect_socket);

    return 1;

}

ThreadedClient::ThreadedClient() {}

int ThreadedClient::_listen_thread(SOCKET server) {
    _connected = 1;
    while (_connected) {

        // recieving message, and then handling for each type

        Message recieved = Message::recieve_message(server);
        std::lock_guard<std::mutex> lock(console_mutex);

        _clear_console_line();

        // handling normal messages and leave and join messages are very straightforward

        switch (recieved.get_type()) {
        case CLIENT_TEXT_MESSAGE:
        case SERVER_MESSAGE:
            std::cout 
                << recieved.get_sender()
                << ": " 
                << recieved.get_body_str()
                << '\n';
            break;
        case CLIENT_LEAVE:
            std::cout << recieved.get_body_str() << " has left the chat." << '\n';
            break;
        case CLIENT_JOIN:
            std::cout << recieved.get_body_str() << " has joined the chat" << '\n';
            break;
        case ERRORMSG:
            // on account of an error, the socket will be closed automatically.
            std::cout << "Encountered an error with code: " << recieved.get_body_int() << endl;
            _connected = 0;
            closesocket(server);
            break;
        case DISCONNECT:
            // other than errors, the socket can only close connections in this way.
            _connected = 0;
            std::cout << "Connection closed due to: " << recieved.get_body_str() << endl;
            closesocket(server);
            break;
        }

        _restore_client_input();
    }

    return 1;
}

void ThreadedClient::_clear_console_line() {
    std::cout << "\r\033[K";
}

void ThreadedClient::_restore_client_input() {
    std::cout << "> " << input;
}

int ThreadedClient::_write_thread(SOCKET server) {
    char ch;
    /*
        A simple cin would suffice but, on the account of another party sending a message while the user is typing
        the input flow will be broken and will cause confusion

        to combat this, whenever a message is recieved, the input line will be cleared (saved to a string)
        the message will be output, and then the input line wiill be restored
    */

    std::cout << "> ";
    while (1) {
        ch = _getch();
        
        // handling backspace key.
        if (ch == '\b') {
            if (!input.empty()) {
                input.pop_back();
                lock_guard<std::mutex> lock(console_mutex);
                _clear_console_line();
                _restore_client_input();
            }
            continue;
        }
        // handling return key
        if (ch == '\r') {
            if (!input.length()) continue;

            if (input[0] == '/') {
                Message cmd = Message(CLIENT_COMMAND);
                input.erase(input.begin());
                stringstream arguments(input);
                std::string command;
                arguments >> command;

                if (command == "exit") {
                    lock_guard<std::mutex> lock(console_mutex);
                    _clear_console_line();
                    input.clear();
                    std::cout << '\n';
                    break;
                }

                std::vector<std::string> argument_vec;
                std::string arg;
                while (arguments >> arg) {
                    argument_vec.push_back(arg);
                }

                nlohmann::json body_json;
                body_json["command"] = command;
                body_json["arg_num"] = (int)argument_vec.size();
                body_json["arguments"] = argument_vec;

                cmd.set_body_json(body_json);

                cmd.send_message(server);


                lock_guard<std::mutex> lock(console_mutex);
                std::cout << "\n> ";
                input.clear();
                std::cout.flush();
                continue;
                
            }

            // else send message through socket
            Message msg = Message(CLIENT_TEXT_MESSAGE);
            msg.set_body_string(input);

            msg.send_message(server);


            lock_guard<std::mutex> lock(console_mutex);
            std::cout << "\n> ";
            input.clear();
            std::cout.flush();
            continue;
        }

        // any other character will directly be added to string.
        input += ch;
        std::lock_guard<std::mutex> lock(console_mutex);
        _clear_console_line();
        _restore_client_input();
    }

    // ending connection, and requesting server to send DISCONNECT message.

    std::cout << "Ending connection." << endl;
    std::cout.flush();
    Message msg = Message(CLIENT_LEAVE);
    msg.set_body_string(" ");
    msg.send_message(server);
    _connected = 0;
    return 1;
}