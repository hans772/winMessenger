#include "server_client.hpp"
#include <vector>
#include <string>
#include <iostream>
#include <sstream>

ServerClient::ServerClient(std::shared_ptr<ChatRoom> room, SOCKET socket, std::string name) : current_room(room) {
	client_socket = socket;
    client_name = name;
}

ServerClient::ServerClient(SOCKET socket) {
    client_socket = socket;
}

SOCKET ServerClient::get_socket() {
    return client_socket;
}

void ServerClient::broadcast( Message msg ) {
    std::lock_guard<std::mutex> lock(*current_room->room_mutex);
    for (SOCKET client : current_room->members) if (client != client_socket) msg.send_message(client);
}

void ServerClient::set_room(std::shared_ptr<ChatRoom> room) {
    current_room = room;
}

std::string ServerClient::get_room_name() {
    return current_room->name;
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
    broadcast(info);
    
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
            broadcast(recieved_message);
            break;

        case CLIENT_COMMAND:
        {
            nlohmann::json cmd_jsn = recieved_message.get_body_json();

            std::cout << "recieved command: " << cmd_jsn["command"].get<std::string>() << " from " << client_name << " with args: \n";
            for (std::string arg : cmd_jsn["arguments"].get<std::vector<std::string>>()) {
                std::cout << arg << '\n';
            }

            handle_command(cmd_jsn);

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

    broadcast(info);

    closesocket(client_socket);
}

void ServerClient::handle_command(nlohmann::json cmd_jsn) {
    std::string command = cmd_jsn["command"].get<std::string>();
    if (command == "subroom") {
        if (!cmd_jsn["arg_num"].get<int>()) {
            Message info = Message(SERVER_MESSAGE);
            info.set_sender("Server");
            std::stringstream ss;
            ss << "Use `/subroom list to list subrooms" << '\n';
            ss << "Use `/subroom join <subroom_name>` to join a subroom" << std::endl;
            info.set_body_string(
                ss.str()
            );

            info.send_message(client_socket);
            return;
        }
        std::vector<std::string> args = cmd_jsn["arguments"].get<std::vector<std::string>>();
        if (args[0] == "join") {
            Message info = Message(CLIENT_LEAVE_ROOM);
            nlohmann::json data;
            data["client"] = client_name;
            data["room_name"] = args[1];
            info.set_body_json(data);
            info.set_sender("Server");

            broadcast(info);

            current_room = current_room->move_member_to_subroom(
                args[1],
                client_socket
            );

            info.set_type(CLIENT_JOIN_ROOM);
            info.set_body_string(client_name);
            info.set_sender("Server");

            broadcast(info);
        }
        else if (args[0] == "list") {
            Message info = Message(SERVER_MESSAGE);
            info.set_sender("Server");
            std::stringstream ss;
            ss << "There are " << current_room->sub_rooms.size() << " available sub-rooms in this current room\n";
            for (std::shared_ptr<SubRoom> room : current_room->sub_rooms) {
                ss << "     " << room->name << " : " << room->members.size() << " members.\n";
            }
            ss << "Use `/subroom join <subroom_name>` to join a subroom" << std::endl;
            info.set_body_string(
                ss.str()
            );

            info.send_message(client_socket);
        }
    }
}