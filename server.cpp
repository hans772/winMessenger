#include <iostream>
#include <WS2tcpip.h>
#include <thread>

#include "server.hpp"
#include "io_server_helper.hpp"
#include "message.hpp"
#include "json.hpp"

using namespace std;

Server::Server(){}
ThreadedServer::ThreadedServer(){}
IOServer::IOServer() {
    crooms_mutex = std::make_shared<std::mutex>();
}

int Server::check(int result, string oper) {
    // checking for WSAErrors
    if (result != 0) {
        cout << "Encountered an error in: `" << oper << "` with code: " << WSAGetLastError() << endl;;
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
                cout << "Unable to create socket." << endl;
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
                cout << "Unable to bind to available sockets." << endl;
                return 0;
            }
            continue;
        }

        cout << "Created a server on port: " << port << endl;
        break;
    }
    // memory management
    freeaddrinfo(result);

    return 1;
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

    int connected = 0;

    // accepting each client and assigning it a thread.
    // the server will accept only the number of connections specified in max_connections
    while (connected < max_connections) {
        SOCKET new_client = INVALID_SOCKET;
        new_client = accept(_listen_socket, NULL, NULL);
        if (new_client == INVALID_SOCKET) cout << "Accept failed with error: " << WSAGetLastError() << '\n';
        else {
            
            //recieves name and chat room from client

            nlohmann::json join_data = Message::recieve_message(new_client).get_body_json();
            std::string client_name = join_data["name"].get<std::string>();
            std::string room_name = join_data["room"].get<std::string>();
            connected++;

            //if chatroom already exists, member is added to that chatroom;
            bool found = false;
            for (int i = 0; i < chat_rooms.size(); i++) {
                if (chat_rooms[i]->name == room_name) {
                    chat_rooms[i]->add_member(new_client);
                    found = true;

                    std::shared_ptr<ServerClient> new_sclient = std::make_shared<ServerClient>(
                        chat_rooms[i], new_client, client_name
                    );

                    cout << "accepted connection: " << connected << '\n';

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

                cout << "accepted connection: " << connected << '\n';

                thread t(&ServerClient::client_thread, new_sclient);
                t.detach();
            }
        }
    }

    // close listen socket after max conns

    closesocket(_listen_socket);
    cout << "closed listen socket" << endl;

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

    // end program

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
    int connected = 0;
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    for (int i = 0; i < sysinfo.dwNumberOfProcessors * 2; ++i)
        CreateThread(nullptr, 0, IOServer::worker_thread, this, 0, nullptr);

    // creating an IOcompletionPort for each client, using the existing IOCP
    // the server will accept only the number of connections specified in max_connections

    while (connected < max_connections) {
        SOCKET new_client = INVALID_SOCKET;
        new_client = accept(_listen_socket, NULL, NULL);
        if (new_client == INVALID_SOCKET) cout << "Accept failed with error: " << WSAGetLastError() << '\n';
        else {

            IOCP_CLIENT_CONTEXT* ctx = IOServerHelper::create_new_context(
                IOCP_CLIENT_CONTEXT::READ, 
                IOCP_CLIENT_CONTEXT::HEAD, 
                new_client, 
                new Message());
            connected++;
            
            HANDLE assoc = CreateIoCompletionPort((HANDLE)new_client, iocp, 0, 0);
            if (!assoc) {
                std::cerr << "CreateIoCompletionPort failed: " << GetLastError() << "\n";
                closesocket(new_client);
                delete[] ctx->transfer_data;
                delete ctx;
                continue;
            }

            DWORD flags = 0;
            int result = WSARecv(new_client, &(ctx->buffer), 1, nullptr, &flags, &(ctx->overlapped), nullptr);
            if (result == SOCKET_ERROR) {
                int err = WSAGetLastError();
                if (err != WSA_IO_PENDING) {
                    std::cerr << "WSARecv failed: " << err << "\n";
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
    cout << "closed listen socket" << endl;

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

    // end program

    WSACleanup();
    return 1;
}


DWORD WINAPI IOServer::worker_thread(LPVOID lpParam) {

    IOServer* server = (IOServer*)lpParam;

    DWORD bytes_recieved;
    ULONG_PTR key;
    IOCP_CLIENT_CONTEXT* context;


    IOServerHelper helper(&server->chat_rooms, server->crooms_mutex);

    while (GetQueuedCompletionStatus(server->iocp, &bytes_recieved, &key, (LPOVERLAPPED *)&context, INFINITE)) {

        if (bytes_recieved == 0) {
            // handle leaving, send client message for leaving
            std::cout << "Client disconnected\n";
            closesocket(context->client);
            delete[] context->transfer_data;
            delete context;
            continue;
        }
        switch (context->operation) {
        case IOCP_CLIENT_CONTEXT::READ:

            context->expected_bytes -= bytes_recieved;
            context->recieved_bytes += bytes_recieved;

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

            if (context->expected_bytes) {
                context->buffer.buf += bytes_recieved;
                context->buffer.len -= bytes_recieved;

                WSASend(context->client, &context->buffer, 1, nullptr, 0, &context->overlapped, nullptr);

                continue;
            }

            helper.handle_write(context);
            break;
        }
        // define what to do for writes;
    }

    std::cout << "thread exited" << std::endl;
    return 0;
}