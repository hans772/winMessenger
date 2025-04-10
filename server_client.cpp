#include "server_client.hpp"
#include "Winsock2.h"
#include "message.hpp"
#include "json.hpp"
#include <vector>
#include <string>
#include <iostream>

ServerClient::ServerClient(std::shared_ptr<ChatRoom> room, SOCKET socket, std::string name) : current_room(room) {
	client_socket = socket;
    client_name = name;
}

void ServerClient::client_thread() {

    /*
        This function deals with each client separately, in a single thread.
        it accepts 2 arguments, one being the client socket to send and recieve information.
        and the other, the reference to the connected clients vector, this is because the server is multithreaded
        and each client gets its own dedicated thread, the connected clients must reflect accurately
        the number of connected clients.
    */

    // Each client on connecting will send a message with client information (currently just name).

    int connected = 1;

    std::cout << "recieved join message from: " << client_name << std::endl;

    Message info = Message(CLIENT_JOIN);
    info.set_body_string(client_name);
    info.set_sender("Server");

    // Broadcasting join message to all other clients
    {
        std::lock_guard<std::mutex> lock(*current_room->room_mutex);
        for (SOCKET client : current_room->members) if (client != client_socket) info.send_message(client);
    }

    Message recieved_message = Message(DEFAULT_MSG);
    Message disc = Message(DISCONNECT);
    disc.set_sender("Server");
    while (connected) {

        // recieve message from client
        recieved_message = Message::recieve_message(client_socket);
        recieved_message.set_sender(client_name);

        // handle each type of message
        switch (recieved_message.get_type()) {

        case CLIENT_TEXT_MESSAGE:

            // on recieving a text message from client, message is broadcasted to every other client.
            std::cout << recieved_message.get_sender() << ": " << recieved_message.get_body_str() << '\n';
            {
                std::lock_guard<std::mutex> lock(*current_room->room_mutex);

                for (SOCKET client : current_room->members) {
                    if (client != client_socket) {
                        recieved_message.send_message(client);
                    }
                }
                break;
            }

        case CLIENT_COMMAND:
        {
            nlohmann::json cmd_jsn = recieved_message.get_body_json();

            std::cout << "recieved command: " << cmd_jsn["command"].get<std::string>() << " from " << client_name << " with args: \n";
            for (std::string arg : cmd_jsn["arguments"].get<std::vector<std::string>>()) {
                std::cout << arg << '\n';
            }

            break;
        }
        case CLIENT_LEAVE:

            // On disconnect, disconnect message is sent to client, to cleanly disconnect from server.
            std::cout << client_name << " Disconnected." << std::endl;
            connected = 0;
            disc.set_body_string("Client Request");
            disc.send_message(client_socket);
            break;

        case ERRORMSG:

            // On error, connection is closed.
            std::cout << "Error in client socket: " << client_name << std::endl;
            connected = 0;
            break;
        }
    }

    // removing socket from connected clients, sending leave message and closing connection

    current_room->remove_member(client_socket);
    info.set_type(CLIENT_LEAVE);

    std::lock_guard<std::mutex> lock(*current_room->room_mutex);
    for (SOCKET client : current_room->members) info.send_message(client);

    closesocket(client_socket);
}