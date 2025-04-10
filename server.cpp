#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>

#include "server.hpp"
#include "message.hpp"
#include "server_client.hpp"

using namespace std;

Server::Server(){}

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

        cout << "Listening on port: " << port << endl;
        break;
    }
    // memory management
    freeaddrinfo(result);

    return 1;
}

ThreadedServer::ThreadedServer(){}

//int ThreadedServer::_client_thread(SOCKET client_socket, vector<SOCKET>& conn_clients) {
//
//    /*
//        This function deals with each client separately, in a single thread.
//        it accepts 2 arguments, one being the client socket to send and recieve information.
//        and the other, the reference to the connected clients vector, this is because the server is multithreaded
//        and each client gets its own dedicated thread, the connected clients must reflect accurately
//        the number of connected clients.
//    */
//
//    // Each client on connecting will send a message with client information (currently just name).
//
//    Message init = Message::recieve_message(client_socket);
//    string client_name = init.get_body_str();
//    init.set_sender("Server");
//   
//    int connected = 1;
//
//    cout << "recieved join message from: " << init.get_body_str() << endl;
//
//    // Broadcasting join message to all other clients
//    for (SOCKET client : conn_clients) if (client != client_socket) init.send_message(client);
//
//    Message recieved_message = Message(DEFAULT_MSG);
//    Message disc = Message(DISCONNECT);
//    disc.set_sender("Server");
//    while (connected) {
//
//        // recieve message from client
//        recieved_message = Message::recieve_message(client_socket);
//        recieved_message.set_sender(client_name);
//
//        // handle each type of message
//        switch (recieved_message.get_type()) {
//
//        case CLIENT_TEXT_MESSAGE:
//
//            // on recieving a text message from client, message is broadcasted to every other client.
//            cout << recieved_message.get_sender() << ": " << recieved_message.get_body_str() << '\n';
//
//            for (SOCKET client : conn_clients) {
//                if (client != client_socket) {
//                    recieved_message.send_message(client);
//                }
//            }
//            break;
//
//        case CLIENT_LEAVE:
//
//            // On disconnect, disconnect message is sent to client, to cleanly disconnect from server.
//            cout << client_name << " Disconnected." << endl;
//            connected = 0;
//            disc.set_body_string("Client Request");
//            disc.send_message(client_socket);
//            break;
//
//        case ERRORMSG:
//
//            // On error, connection is closed.
//            cout << "Error in client socket: " << client_name << endl;
//            connected = 0;
//            break;
//        }
//    }
//
//    // removing socket from connected clients, sending leave message and closing connection
//
//    conn_clients.erase(find(conn_clients.begin(), conn_clients.end(), client_socket));
//    init.set_type(CLIENT_LEAVE);
//    for (SOCKET client : conn_clients) init.send_message(client);
//
//    closesocket(client_socket);
//
//    return 1;
//}

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

            std::string client_name = Message::recieve_message(new_client).get_body_str();
            std::string room_name = Message::recieve_message(new_client).get_body_str();
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

                    chat_rooms[i]->add_member(new_client);

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