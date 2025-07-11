#include <iostream>
#include <WS2tcpip.h>
#include <thread>

#include "server.hpp"
#include "io_server_helper.hpp"
#include "message.hpp"
#include "json.hpp"
#include "logger.hpp"


using namespace std;

Server::Server() { connected = 0; auth = std::make_shared<ServerAuth>(); }
ThreadedServer::ThreadedServer(){}
IOServer::IOServer() {
    crooms_mutex = std::make_shared<std::mutex>();
}

int Server::check(int result, string oper) {
    // checking for WSAErrors
    if (result != 0) {
        Logger::get().log(LogLevel::ERR, LogModule::SERVER, "Operation: ", oper, "Failed with Code:", WSAGetLastError());
        return 0;
    }
    return 1;
}

int Server::create_tcp_socket(const char *port) {

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

    // Get addr info will replace result with a linked list of available socket information.

    if (!check(getaddrinfo(NULL, port, &hints, &result), "GetADDRINFO")) {
        return 0;
    }

    _listen_socket = INVALID_SOCKET;

    ptr = result;
    // Loop through result to create an available socket for accepting connections.

    while (ptr != NULL) {

        // If socket creation fails, move to next available node.

        _listen_socket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

        if (_listen_socket == INVALID_SOCKET) {
            ptr = ptr->ai_next;
            if (ptr == NULL) {
                Logger::get().log(LogLevel::ERR, LogModule::SERVER, "Unable to Create Socket");

                return 0;
            }
            continue;
        }

        // If binding fails, move to next available node.

        if (!check(::bind(_listen_socket, ptr->ai_addr, (int)ptr->ai_addrlen), "Socket Bind")) {
            closesocket(_listen_socket);
            _listen_socket = INVALID_SOCKET;

            ptr = ptr->ai_next;
            if (ptr == NULL) {
                Logger::get().log(LogLevel::ERR, LogModule::SERVER, "Unable to Bind to Socket");

                return 0;
            }
            continue;
        }

        Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Created Server on Port: ", port);

        break;
    }
    // memory management
    freeaddrinfo(result);

    return 1;
}

// validates token, same method as event server (check below), except this is threaded

std::string ThreadedServer::validate_join(nlohmann::json join_data, SOCKET new_client) { 
    if (join_data.contains("tok")) {
        if (auth->verify_token(join_data["tok"].get<std::string>())) {
            std::string tok = join_data["tok"].get<std::string>();
            return tok.substr(0, tok.find('.'));
        }
        else {
            Message errmsg = Message(CLIENT_INVALID_TOKEN);
            errmsg.set_body_int(401);
            errmsg.send_message(new_client);

            return "";
        }
    }
    else {
        std::string name = join_data["name"];
        if (!(auth->check_user(name))) {
            auth->add_user(name);
            return name;
        }
        else {
            Message errmsg = Message(CLIENT_INVALID_USERNAME);
            errmsg.set_body_string(join_data["room"]);

            errmsg.send_message(new_client);

            return validate_join(Message::recieve_message(new_client).get_body_json(), new_client);
        }
    }
}
int ThreadedServer::listen_and_accept(int max_connections) {

    /*
        This function lets the server listen for incoming connections, each incoming connection is handled
        by the server, each client is assigned a socket and is then handled in a separate thread.
    */

    // checking if socket is available for listening
    if (!check(listen(_listen_socket, SOMAXCONN), "Listen")) {
        closesocket(_listen_socket);
        return 0;
    }

    // accepting each client and assigning it a thread.
    // the server will accept only the number of connections specified in max_connections
    while (connected < max_connections) {
        SOCKET new_client = INVALID_SOCKET;
        new_client = accept(_listen_socket, NULL, NULL);
        if (new_client == INVALID_SOCKET) Logger::get().log(LogLevel::ERR, LogModule::NETWORK, "Accept Failed with Error: ", WSAGetLastError());
        else {
            
            //recieves name and chat room from client

            nlohmann::json join_data = Message::recieve_message(new_client).get_body_json();
            std::string client_name;
            std::string room_name = join_data["room"].get<std::string>();
            // validates the join and only then accepts, if returned name is empty, ie invalid token, then accept is skipped
            if ((client_name = validate_join(join_data, new_client)).length()) {
                connected++;

                Message setupmsg = Message(CLIENT_SETUP);
                nlohmann::json setupjson;
                setupjson["username"] = client_name;
                setupjson["room"] = join_data["room"];
                if (!join_data.contains("token")) setupjson["token"] = auth->get_token(client_name);

                setupmsg.set_body_json(setupjson);
                setupmsg.send_message(new_client);
            }
            else {
                continue;
            }

            //if chatroom already exists, member is added to that chatroom;
            bool found = false;
            for (int i = 0; i < chat_rooms.size(); i++) {
                if (chat_rooms[i]->name == room_name) {
                    chat_rooms[i]->add_member(new_client);
                    found = true;

                    std::shared_ptr<ServerClient> new_sclient = std::make_shared<ServerClient>(
                        chat_rooms[i], new_client, client_name
                    );

                    Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Accepted Connection");

                    thread t(&ServerClient::client_thread, new_sclient);
                    t.detach();
                    break;
                }
            }
            if (!found) {

                // else a new chat room is created

                std::shared_ptr<ChatRoom> new_room = std::make_shared<ChatRoom>(room_name);
                chat_rooms.push_back(new_room);

                std::shared_ptr<ServerClient> new_sclient = std::make_shared<ServerClient>(
                    new_room , new_client, client_name
                );

                new_room->add_member(new_client);  

                Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Accepted Connection");

                thread t(&ServerClient::client_thread, new_sclient);
                t.detach();
            }
        }
    }

    // close listen socket after max conns

    closesocket(_listen_socket);
    Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Closed Listen Socket");

    // wait for all connections to disconnect

    while (chat_rooms.size()) {
        // checking only every 10 seconds to reduce load on the CPU.
        std::vector<int> empties;
        for (int i = 0; i < chat_rooms.size(); i++) {
            if (!chat_rooms[i]->members.size()) {
                empties.push_back(i);
            }
        }
        for (int i = empties.size() - 1; i >= 0; i--) {
            chat_rooms.erase(chat_rooms.begin() + empties[i]);
        }
        this_thread::sleep_for(chrono::milliseconds(10000));
    }

    WSACleanup();
    return 1;
}

int IOServer::listen_and_accept(int max_connections) {

    /*
        This function lets the server listen for incoming connections, each incoming connection is handled
        by the server, each client is assigned a socket and is then handled in a separate thread.
    */

    // checking if socket is available for listening
    if (!check(listen(_listen_socket, SOMAXCONN), "Listen")) {
        closesocket(_listen_socket);
        return 0;
    }

    iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    int thread_count = sysinfo.dwNumberOfProcessors * 2; // creates a number of threads (number of processors x 2 )
    for (int i = 0; i < thread_count; ++i)
        CreateThread(nullptr, 0, IOServer::worker_thread, this, 0, nullptr);

    // creating an IOcompletionPort for each client, using the existing IOCP
    // the server will accept only the number of connections specified in max_connections

    while (connected < max_connections) {
        SOCKET new_client = INVALID_SOCKET;
        new_client = accept(_listen_socket, NULL, NULL);
        if (new_client == INVALID_SOCKET) Logger::get().log(LogLevel::ERR, LogModule::NETWORK, "Accept Failed with Error: " , WSAGetLastError());
        else {

            // creates a read context, this will be reused for the same client
            IOCP_CLIENT_CONTEXT* ctx = IOServerHelper::create_new_context(
                IOCP_CLIENT_CONTEXT::READ, 
                IOCP_CLIENT_CONTEXT::HEAD, 
                new_client, 
                new Message());

            // tells the computer to check for reads and writes to and from this socket and on completion, send to worker thread
            HANDLE assoc = CreateIoCompletionPort((HANDLE)new_client, iocp, 0, 0);
            if (!assoc) {
                Logger::get().log(LogLevel::ERR, LogModule::SERVER, "CICP Failed");
                closesocket(new_client);
                delete[] ctx->transfer_data;
                delete ctx;
                continue;
            }
            connected++;
            Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Accepted Connection");
            DWORD flags = 0;
            // sends first read, creates a chain of reads until terminated
            int result = WSARecv(new_client, &(ctx->buffer), 1, nullptr, &flags, &(ctx->overlapped), nullptr);
            if (result == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSA_IO_PENDING) {
                    Logger::get().log(LogLevel::ERR, LogModule::SERVER, "WSARecv Failed");
                    closesocket(new_client);
                    delete[] ctx->transfer_data;
                    delete ctx;
                    continue;
                }
            }
        }
    }

    // close listen socket after max conns

    closesocket(_listen_socket);
    Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Closed Listen Socket");

    // wait for all connections to disconnect

    while (connected) {
        // checking only every 10 seconds to reduce load on the CPU.
        std::vector<int> empties;
        for (int i = 0; i < chat_rooms.size(); i++) {
            if (!chat_rooms[i]->member_count) {
                empties.push_back(i);
            }
        }
        for (int i = empties.size() - 1; i >= 0; i--) {
            chat_rooms.erase(chat_rooms.begin() + empties[i]);
        }
        this_thread::sleep_for(chrono::milliseconds(10000));
    }

    // end program

    for (int i = 0; i < thread_count; ++i) {
        PostQueuedCompletionStatus(iocp, 0, 0, nullptr); // nullptr context = exit signal
    }

    WSACleanup();
    return 1;
}


DWORD WINAPI IOServer::worker_thread(LPVOID lpParam) {

    IOServer* server = (IOServer*)lpParam;

    DWORD bytes_recieved;
    ULONG_PTR key;
    IOCP_CLIENT_CONTEXT* context;

    // initializes a helper
    IOServerHelper helper(&server->chat_rooms, server->crooms_mutex, server->auth);

    while (true) {

        BOOL ok = GetQueuedCompletionStatus(server->iocp, &bytes_recieved, &key, (LPOVERLAPPED*)&context, INFINITE);
        // if ok is false it means something went wrong
        if (!ok) {
            DWORD err = WSAGetLastError();
            if (err == SOCKET_ERROR || err == 64) {
                Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Client Terminated Connection");
                server->connected--;
                if (context->room) {
                    context->room->reduce_count();
                    context->room.reset();
                }
                closesocket(context->client);
                delete[] context->transfer_data;
                delete context;
            }
            Logger::get().log(LogLevel::ERR, LogModule::NETWORK, "GQCS Failed with Error", WSAGetLastError());
            // making sure that thread doesnt end due to errors, otherwise server will shut down for forceful client disconnects.. etc
            continue;
        }

        if (context == nullptr) { // this is the exit signal sent by the server main thread, to indicate that the server has closed
            Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Worker Thread Exited");
            break;
        }

        // if bytes recieved is 0, client has disconnected
        if (bytes_recieved == 0) {
            Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Client Disconnected");
            server->connected--;
            if (context->room) {
                context->room->reduce_count();
                context->room.reset();
            }
            closesocket(context->client);
            delete[] context->transfer_data;
            delete context;
            continue;
        }
        switch (context->operation) {
        case IOCP_CLIENT_CONTEXT::READ:

            context->expected_bytes -= bytes_recieved;
            context->recieved_bytes += bytes_recieved;

            // for each section, a specific number of bytes is expected, if all those bytes are not recieved at once
            // the worker thread asks the server to recieve the remaining bytes and then only handles the read;
            if (context->expected_bytes) {
                context->buffer.buf = context->transfer_data + bytes_recieved;
                context->buffer.len = context->expected_bytes;

                DWORD flags = 0;
                WSARecv(context->client, &(context->buffer), 1, nullptr, &flags, &(context->overlapped), nullptr);

                continue;
            }
            helper.handle_read(context);

            break;
        case IOCP_CLIENT_CONTEXT::WRITE:
            context->expected_bytes -= bytes_recieved;
            context->recieved_bytes += bytes_recieved;

            // same for writing

            if (context->expected_bytes) {
                context->buffer.buf += bytes_recieved;
                context->buffer.len -= bytes_recieved;

                WSASend(context->client, &context->buffer, 1, nullptr, 0, &context->overlapped, nullptr);

                continue;
            }

            helper.handle_write(context);
            break;
        }
    }
    return 0;
}