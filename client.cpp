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
#include "logger.hpp"

using namespace std;
std::mutex console_mutex;

Client::Client() { auth = ClientAuth(); }

int Client::check(int result, string oper) {
    // checking for WSAerrors
    if (result != 0) {
        Logger::get().log(LogLevel::ERR, LogModule::CLIENT, "Operation: ", oper, "Failed with Code:", WSAGetLastError());

        return 0;
    }
    return 1;
}

int Client::create_tcp_socket(const char* ip, const char* port, std::string ip_str) {

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
                Logger::get().log(LogLevel::ERR, LogModule::CLIENT, "Unable to Create Socket");

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
                Logger::get().log(LogLevel::ERR, LogModule::CLIENT, "Unable to Connect to Server");

                return 0;
            }
            continue;
        }

        Logger::get().log(LogLevel::INFO, LogModule::CLIENT, "Connected to Server on Port: ", port);
        connected_ip = ip_str;
        break;
    }

    // memory management

    freeaddrinfo(result);
    _connected = 1;

    return 1;
}

int ThreadedClient::start_chat() {

    nlohmann::json body;
    // checks if a token already exists for the connected ip
    if (auth.check_token(connected_ip)) {
        body["tok"] = auth.get_token(connected_ip);
    }
    else { // otherwise asks for name
        string name;
        std::cout << "Enter your name: ";
        cin >> name;
        body["name"] = name;

    }
    // asks for which room the user wishes to join

    string room;
    std::cout << "Enter the room you wish to join: ";
    cin >> room;

    body["room"] = room;

    // creating a join message to send to server as initial message

    Message msg = Message(CLIENT_JOIN);
    msg.set_body_json(body);
    msg.send_message(_connect_socket);

    // creating a write thread, one can also create a listen_thread if they intend on doing other things on the main thread

    //thread listen(&ThreadedClient::_listen_thread, this, _connect_socket);
    thread write(&ThreadedClient::_write_thread, this, _connect_socket);

    // write runs in parallel
    write.detach();
    _listen_thread(_connect_socket);

    return 1;

}

ThreadedClient::ThreadedClient() { client_verified = 0; }

int ThreadedClient::_listen_thread(SOCKET server) {
    _connected = 1;
    while (_connected) {

        // recieving message, and then handling for each type

        Message recieved = Message::recieve_message(server);
        std::lock_guard<std::mutex> lock(console_mutex);

        _clear_console_line();

        // handling normal messages and leave and join messages are very straightforward

        switch (recieved.get_type()) {

            // client setup is sent when a user is successfully accepted and added to a room

        case CLIENT_SETUP:
            std::cout << "\nWELCOME    :    " << recieved.get_body_json()["username"].get<std::string>() << "\n\n";
            joined_room = recieved.get_body_json()["room"];
            std::cout << "CHATROOM: " << joined_room << "\n" << endl;

            // creates a token if not already there

            if (recieved.get_body_json().contains("tok")) {
                auth.set_token(recieved.get_body_json()["tok"].get<std::string>(), connected_ip);
            }
            client_verified = 1;
            break;
            
            //CLIENT_INVALID_USERNAME is recieved when a username which was already in use was sent to the server
            //This can onl be recieved before client_setup, so we do not need any input operations
            //The write thread will be asleep until client setup is recieved so we can safely use cin to get another userna,e
        case CLIENT_INVALID_USERNAME: {
            std::cout << "That username already exists, please select another one:\n";
            std::string name;
            std::cin >> name;
            nlohmann::json body;
            body["name"] = name;
            body["room"] = recieved.get_body_str(); // just chaining the room name sent previously, avoiding the need for another input

            Message msg = Message(CLIENT_JOIN);
            msg.set_body_json(body);
            msg.send_message(_connect_socket);

            break;
        }
            // This message can only be recieved if the token has been tampered with, client will disconnect.

        case CLIENT_INVALID_TOKEN:
            Logger::get().log(LogLevel::ERR, LogModule::AUTH, "Authentication Token Corrupted");
            _connected = 0;

          
        case CLIENT_TEXT_MESSAGE:
        case SERVER_MESSAGE:
            std::cout 
                << recieved.get_sender()
                << ": " 
                << recieved.get_body_str()
                << '\n';
            break;

            // client leaving will be notified with this message
        case CLIENT_LEAVE:
            std::cout << recieved.get_body_str() << " has left the chat." << '\n';
            break;
        case CLIENT_JOIN:
            std::cout << recieved.get_body_str() << " has joined the chat" << '\n';
            break;
        case CLIENT_JOIN_ROOM:
            std::cout << recieved.get_body_str() << " has joined the room." << '\n';
            break;
        case CLIENT_LEAVE_ROOM: {
            nlohmann::json data = recieved.get_body_json();
            std::cout
                << data["client"].get<std::string>()
                << " has left the room to room: "
                << data["room_name"].get<std::string>()
                << '\n';
            break;
        }
        case ERRORMSG:
            // on account of an error, the socket will be closed automatically.
            Logger::get().log(LogLevel::INFO, LogModule::CLIENT, "Recieved an Error Message with code: ", recieved.get_body_int());

            _connected = 0;
            closesocket(server);
            break;
        case DISCONNECT:
            // other than errors, the socket can only close connections in this way.
            // it is a warning saying that the server will be closing the socket after this message.
            _connected = 0;
            Logger::get().log(LogLevel::INFO, LogModule::CLIENT, "Connection Closed due to: ", recieved.get_body_str());

            closesocket(server);
            break;
        }

        _restore_client_input();
    }

    return 1;
}

void ThreadedClient::_clear_console_line() {
    std::cout << "\r\033[K"; //this might not work on all systems.
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

    while (!client_verified) {
        this_thread::sleep_for(chrono::milliseconds(2000)); // the write thread is asleep till client setup is recieved.
    }

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

            if (input[0] == '/') { // handling commands
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

    Logger::get().log(LogLevel::INFO, LogModule::CLIENT, "Ending Connection");

    std::cout.flush();
    Message msg = Message(CLIENT_LEAVE);
    msg.set_body_string(" ");
    msg.send_message(server);
    _connected = 0;
    return 1;
}