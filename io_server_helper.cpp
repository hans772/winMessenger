
#include "io_server_helper.hpp"
#include <string>
#include <sstream>

IOCP_CLIENT_CONTEXT* IOServerHelper::create_new_context(
    IOCP_CLIENT_CONTEXT::Operation oper,
    IOCP_CLIENT_CONTEXT::Part part,
    SOCKET socket,
    Message* message) {

    IOCP_CLIENT_CONTEXT* wctx = new IOCP_CLIENT_CONTEXT;
    ZeroMemory(&(wctx->overlapped), sizeof(OVERLAPPED));
    wctx->transfer_data = new char[sizeof(int32_t)];
    wctx->buffer.buf = wctx->transfer_data;
    wctx->buffer.len = wctx->expected_bytes = sizeof(int32_t);
    wctx->operation = oper;
    wctx->part = part;
    wctx->client = socket;
    wctx->name = "???";
    wctx->transfer_message = message;

    return wctx;
}

void IOServerHelper::set_context_for_message(IOCP_CLIENT_CONTEXT *context) {
    std::pair<std::string, int> headerinfo = context->transfer_message->serialize_header();

    int32_t headerlen = htonl(headerinfo.second);
    memcpy(context->transfer_data, &headerlen, sizeof(int32_t));

}

IOServerHelper::IOServerHelper(std::vector<std::shared_ptr<ChatRoom>>* rooms, std::shared_ptr<std::mutex> mutex) {
    server_rooms = rooms;
    server_mutex = mutex;
}

void IOServerHelper::broadcast(IOCP_CLIENT_CONTEXT * context, Message message) {
    std::lock_guard<std::mutex> lock(*(context->room->room_mutex));
    for (SOCKET client : context->room->members) if (client != context->client) {
        IOCP_CLIENT_CONTEXT* wctx = IOServerHelper::create_new_context(
            IOCP_CLIENT_CONTEXT::WRITE, 
            IOCP_CLIENT_CONTEXT::HEAD, 
            client, 
            new Message(message)
        );
        IOServerHelper::set_context_for_message(wctx);
        WSASend(client, &wctx->buffer, 1, nullptr, 0, &wctx->overlapped, nullptr);
    }
}

void IOServerHelper::handle_read(IOCP_CLIENT_CONTEXT* context) {
    switch (context->part) {
    case IOCP_CLIENT_CONTEXT::HEAD:
        context->expected_bytes = ntohl(*(int32_t*)(context->transfer_data));
        context->part = IOCP_CLIENT_CONTEXT::HEADER_JS;
        break;
    case IOCP_CLIENT_CONTEXT::HEADER_JS: {
        nlohmann::json header = Message::deserialize_header(
            std::string(context->transfer_data, context->recieved_bytes)
        );
        context->transfer_message->set_type((MessageType)header["message_type"].get<int>());
        context->expected_bytes = header["body_length"].get<int>();
        context->transfer_message->set_sender(header["sender"].get<std::string>());
        context->transfer_message->set_body_type(header["body_type"].get<int>());
        context->part = IOCP_CLIENT_CONTEXT::BODY;
        break;
    }
    case IOCP_CLIENT_CONTEXT::BODY:
        context->transfer_message->set_body_from_buffer(
            context->transfer_message->get_body_type(),
            context->transfer_data,
            context->recieved_bytes
        );

        delete[] context->transfer_data;


        handle_message(context);
        context->transfer_data = new char[sizeof(int32_t)];
        context->buffer.buf = context->transfer_data;
        context->buffer.len = context->expected_bytes = sizeof(int32_t);
        context->part = IOCP_CLIENT_CONTEXT::HEAD;

        break;
    }
    if( context->part != IOCP_CLIENT_CONTEXT::HEAD) { // if it is HEAD then it means the BODY just got processed
        delete[] context->transfer_data;

        context->recieved_bytes = 0;
        context->transfer_data = new char[context->expected_bytes];
        context->buffer.buf = context->transfer_data;
        context->buffer.len = context->expected_bytes;
    }

    DWORD flags = 0;
    WSARecv(context->client, &(context->buffer), 1, nullptr, &flags, &(context->overlapped), nullptr);

}

void IOServerHelper::handle_write(IOCP_CLIENT_CONTEXT* context) {
    switch (context->part) {
    case IOCP_CLIENT_CONTEXT::HEAD: {
        std::pair<std::string, int> headerinfo = context->transfer_message->serialize_header();

        context->expected_bytes = headerinfo.second;
        context->transfer_data = new char[context->expected_bytes];
        strncpy(context->transfer_data, headerinfo.first.c_str(), headerinfo.first.length());


        context->part = IOCP_CLIENT_CONTEXT::HEADER_JS;
        break;
    }
    case IOCP_CLIENT_CONTEXT::HEADER_JS: {
        delete[] context->transfer_data;

        context->expected_bytes = context->transfer_message->get_header()["body_length"].get<int>();
        context->transfer_data = new char[context->expected_bytes];


        switch (context->transfer_message->get_header()["body_type"].get<int>()) {
        case MSGBODY_STRING:
            memcpy(context->transfer_data, context->transfer_message->get_body_str().c_str(), context->expected_bytes);
            break;
        case MSGBODY_INT:
            context->transfer_data = new char[sizeof(int32_t)];
            memcpy(context->transfer_data, new int32_t(context->transfer_message->get_body_int32()), sizeof(int32_t));
            break;
        case MSGBODY_JSON:
            memcpy(context->transfer_data, context->transfer_message->get_body_json().dump().c_str(), context->expected_bytes);
            break;
        default:
            std::cout << "Unknown body type" << std::endl;
            return;
        }

        context->part = IOCP_CLIENT_CONTEXT::BODY;
        break;
    }
    case IOCP_CLIENT_CONTEXT::BODY:
        delete context->transfer_message;
        delete[] context->transfer_data;
        return;
    }
    context->recieved_bytes = 0;
    context->buffer.buf = context->transfer_data;
    context->buffer.len = context->expected_bytes;
    WSASend(context->client, &context->buffer, 1, nullptr, 0, &context->overlapped, nullptr);

}

void IOServerHelper::handle_message(IOCP_CLIENT_CONTEXT * context) {

    std::lock_guard<std::mutex> lock(*server_mutex);

    switch (context->transfer_message->get_type()) {
    case (int)MessageType::CLIENT_JOIN: {
        std::cout << context->transfer_message->get_body_type() << std::endl;

        context->name = context->transfer_message->get_body_json()["name"].get<std::string>();
        std::string room_name = context->transfer_message->get_body_json()["room"].get<std::string>();

        //if chatroom already exists, member is added to that chatroom;
        bool found = false;
        for (int i = 0; i < server_rooms->size(); i++) {
            if ((*server_rooms)[i]->name == room_name) {
                context->room = (*server_rooms)[i];
                (*server_rooms)[i]->add_member(context->client);

                found = true;

                break;
            }
        }
        if (!found) {

            // else a new chat room is created

            std::shared_ptr<ChatRoom> new_room = std::make_shared<ChatRoom>(room_name);
            server_rooms->push_back(new_room);
            context->room = new_room;
            new_room->add_member(context->client);

        }
        broadcast(context, *(context->transfer_message));
    }
    case MessageType::CLIENT_TEXT_MESSAGE:

        // on recieving a text message from client, message is broadcasted to every other client.
        std::cout << context->transfer_message->get_sender() << ": " << context->transfer_message->get_body_str() << '\n';

        broadcast(context, *(context->transfer_message));
        break;

    case MessageType::CLIENT_COMMAND:
    {
        nlohmann::json cmd_jsn = context->transfer_message->get_body_json();

        std::cout << "recieved command: " << cmd_jsn["command"].get<std::string>() << " from " << context->name << " with args: \n";
        for (std::string arg : cmd_jsn["arguments"].get<std::vector<std::string>>()) {
            std::cout << arg << '\n';
        }
        handle_command(cmd_jsn, context);

        break;
    }
    case MessageType::CLIENT_LEAVE:
    {

        // On disconnect, disconnect message is sent to client, to cleanly disconnect from server.
        Message* disc = new Message(CLIENT_LEAVE);
        disc->set_body_string("Client Request");
        std::cout << context->name << " Disconnected." << std::endl;
        IOCP_CLIENT_CONTEXT* wctx = IOServerHelper::create_new_context(
            IOCP_CLIENT_CONTEXT::WRITE,
            IOCP_CLIENT_CONTEXT::HEAD,
            context->client,
            disc
        );
        IOServerHelper::set_context_for_message(wctx);
        WSASend(context->client, &wctx->buffer, 1, nullptr, 0, &wctx->overlapped, nullptr);
    }

    case MessageType::ERRORMSG:
    {

        Message info;
        if (context->transfer_message->get_type() == MessageType::ERRORMSG) std::cout << "Error in client socket: " << context->name << std::endl;
        context->room->remove_member(context->client);
        info.set_type(CLIENT_LEAVE);

        context->room.reset();

        broadcast(context, info);
        break;
    }
    }
}

void IOServerHelper::handle_command(nlohmann::json cmd_jsn, IOCP_CLIENT_CONTEXT *context) {
    std::string command = cmd_jsn["command"].get<std::string>();
    if (command == "subroom") {
        if (!cmd_jsn["arg_num"].get<int>()) {
            Message *info = new Message(SERVER_MESSAGE);
            info->set_sender("Server");
            std::stringstream ss;
            ss << "Use `/subroom list to list subrooms" << '\n';
            ss << "Use `/subroom join <subroom_name>` to join a subroom" << std::endl;
            info->set_body_string(
                ss.str()
            );

            IOCP_CLIENT_CONTEXT* wctx = IOServerHelper::create_new_context(
                IOCP_CLIENT_CONTEXT::WRITE,
                IOCP_CLIENT_CONTEXT::HEAD,
                context->client,
                info
            );
            WSASend(context->client, &wctx->buffer, 1, nullptr, 0, &wctx->overlapped, nullptr);
            return;
        }
        std::vector<std::string> args = cmd_jsn["arguments"].get<std::vector<std::string>>();
        if (args[0] == "join") {
            Message info = Message(CLIENT_LEAVE_ROOM);
            nlohmann::json data;
            data["client"] = context->name;
            data["room_name"] = args[1];
            info.set_body_json(data);
            info.set_sender("Server");

            broadcast(context, info);

            context->room = context->room->move_member_to_subroom(
                args[1],
                context->client
            );

            info.set_type(CLIENT_JOIN_ROOM);
            info.set_body_string(context->name);
            info.set_sender("Server");

            broadcast(context, info);
        }
        else if (args[0] == "list") {
            Message info = Message(SERVER_MESSAGE);
            info.set_sender("Server");
            std::stringstream ss;
            ss << "There are " << context->room->sub_rooms.size() << " available sub-rooms in this current room\n";
            for (std::shared_ptr<SubRoom> room : context->room->sub_rooms) {
                ss << "     " << room->name << " : " << room->members.size() << " members.\n";
            }
            ss << "Use `/subroom join <subroom_name>` to join a subroom" << std::endl;
            info.set_body_string(
                ss.str()
            );

            IOCP_CLIENT_CONTEXT* wctx = IOServerHelper::create_new_context(
                IOCP_CLIENT_CONTEXT::WRITE,
                IOCP_CLIENT_CONTEXT::HEAD,
                context->client,
                new Message(info)
            );
            WSASend(context->client, &wctx->buffer, 1, nullptr, 0, &wctx->overlapped, nullptr);
        }
    }
}