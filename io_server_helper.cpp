
#include "io_server_helper.hpp"
#include <string>
#include <sstream>
#include "logger.hpp"
#include "auth.hpp"

// EVERY WRITE OPERATION REQUIRES A NEW CONTEXT, WHICH WILL BE DELETED AFTER EVERY WRITE

// EVERY READ OPERATION CALLS ANOTHER READ OPERATION FROM THE SAME SOCKET AFTER EACH COMPLETION, HELPS MAINTAIN CONSTANT CONNECTION


// it creates a context for a specific operation for a specific part of a given message for a given socket.

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

    int32_t headerlen = htonl(headerinfo.second); // length of the header so that it can be sent over first
    memcpy(context->transfer_data, &headerlen, sizeof(int32_t));

}

// creates a helper and initializes the shared pointers
IOServerHelper::IOServerHelper(std::vector<std::shared_ptr<ChatRoom>>* rooms, std::shared_ptr<std::mutex> mutex, std::shared_ptr<ServerAuth> auth) : auth(auth) {
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
    case IOCP_CLIENT_CONTEXT::HEAD: // if part read is head, it sets info and prepares for recieving of header
        context->expected_bytes = ntohl(*(int32_t*)(context->transfer_data));
        context->part = IOCP_CLIENT_CONTEXT::HEADER_JS;
        break;
    case IOCP_CLIENT_CONTEXT::HEADER_JS: { // if part recieved is header, it sets info and prepares for recieving of body
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
    case IOCP_CLIENT_CONTEXT::BODY: // if recieved part is body sets data and handles the message
        context->transfer_message->set_body_from_buffer(
            context->transfer_message->get_body_type(),
            context->transfer_data,
            context->recieved_bytes
        );

        delete[] context->transfer_data; // deletes transfer data as it is not necessary anmore


        handle_message(context);
        context->transfer_data = new char[sizeof(int32_t)];
        context->buffer.buf = context->transfer_data;
        context->buffer.len = context->expected_bytes = sizeof(int32_t);
        context->part = IOCP_CLIENT_CONTEXT::HEAD; // context is then set to head for the next recieve option

        break;
    }
    if( context->part != IOCP_CLIENT_CONTEXT::HEAD) { // if it is HEAD then it means the BODY just got processed
        delete[] context->transfer_data; // deletes transfer data to create a new data storage of expected length

        context->recieved_bytes = 0;
        context->transfer_data = new char[context->expected_bytes];
        context->buffer.buf = context->transfer_data;
        context->buffer.len = context->expected_bytes;
    }

    // another recieve operation is sent

    DWORD flags = 0;
    if(context->client )
    WSARecv(context->client, &(context->buffer), 1, nullptr, &flags, &(context->overlapped), nullptr);

}

void IOServerHelper::handle_write(IOCP_CLIENT_CONTEXT* context) {
    switch (context->part) {
    case IOCP_CLIENT_CONTEXT::HEAD: { // if head is written, prepares for headerlen
        std::pair<std::string, int> headerinfo = context->transfer_message->serialize_header();

        context->expected_bytes = headerinfo.second;
        context->transfer_data = new char[context->expected_bytes];
        strncpy(context->transfer_data, headerinfo.first.c_str(), headerinfo.first.length());


        context->part = IOCP_CLIENT_CONTEXT::HEADER_JS;
        break;
    }
    case IOCP_CLIENT_CONTEXT::HEADER_JS: { // if header is written, prepares for body
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
            Logger::get().log(LogLevel::ERR, LogModule::SERVER, "Invalid Body Type for Message Provided");
            return;
        }

        context->part = IOCP_CLIENT_CONTEXT::BODY;
        break;
    }
    case IOCP_CLIENT_CONTEXT::BODY: // if body is sent, deletes the context data
        delete context->transfer_message;
        delete[] context->transfer_data;
        delete context;
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
    case (int)MessageType::CLIENT_JOIN: { // a client sends this message when it attempts to join

        nlohmann::json msg_jsn = context->transfer_message->get_body_json();
        if (msg_jsn.contains("tok")) {
            if (auth->verify_token(msg_jsn["tok"].get<std::string>())) {
                std::string tok = msg_jsn["tok"].get<std::string>();
                context->name = tok.substr(0, tok.find('.'));
            } // if token exists, verifies the token and if verified sets client name
            else {
                // unverfied token, error message is sent

                IOCP_CLIENT_CONTEXT* wctx = IOServerHelper::create_new_context(
                    IOCP_CLIENT_CONTEXT::WRITE,
                    IOCP_CLIENT_CONTEXT::HEAD,
                    context->client,
                    new Message(CLIENT_INVALID_TOKEN)
                );
                wctx->transfer_message->set_body_int(401);
                IOServerHelper::set_context_for_message(wctx);
                WSASend(context->client, &wctx->buffer, 1, nullptr, 0, &wctx->overlapped, nullptr);
                return;
            }
        }
        else { // if token is not sent, name must be sent hence checks if name exists or not and replies accordingly
            std::string name = msg_jsn["name"];
            if (!(auth->check_user(name))) {
                context->name = name;
                auth->add_user(name);
            }
            else {
                IOCP_CLIENT_CONTEXT* wctx = IOServerHelper::create_new_context(
                    IOCP_CLIENT_CONTEXT::WRITE,
                    IOCP_CLIENT_CONTEXT::HEAD,
                    context->client,
                    new Message(CLIENT_INVALID_USERNAME)
                );
                wctx->transfer_message->set_body_string(msg_jsn["room"]);

                IOServerHelper::set_context_for_message(wctx);
                WSASend(context->client, &wctx->buffer, 1, nullptr, 0, &wctx->overlapped, nullptr);

                return;
            }
        }
        std::string room_name = context->transfer_message->get_body_json()["room"].get<std::string>();

        nlohmann::json client_setup;
        client_setup["username"] = context->name;
        client_setup["room"] = msg_jsn["room"];
        if (!msg_jsn.contains("tok")) {
            client_setup["tok"] = auth->get_token(context->name); // if token was not recieved, creates and sends a token
        }

        Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Accepted Client: ", context->name);


        //if chatroom already exists, member is added to that chatroom;
        bool found = false;
        for (int i = 0; i < server_rooms->size(); i++) {
            if ((*server_rooms)[i]->name == room_name) {
                context->room = (*server_rooms)[i];
                context->room->add_member(context->client);
                context->room->add_count();
                found = true;

                break;
            }
        }
        if (!found) {

            // else a new chat room is created

            std::shared_ptr<ChatRoom> new_room = std::make_shared<ChatRoom>(room_name);
            server_rooms->push_back(new_room);
            context->room = new_room;
            context->room->add_count();
            new_room->add_member(context->client);

        }

        Message info(CLIENT_JOIN);
        info.set_body_string(context->name);
        broadcast(context, info);

        // a client join message is created and sent to every client

        IOCP_CLIENT_CONTEXT* wctx = IOServerHelper::create_new_context(
            IOCP_CLIENT_CONTEXT::WRITE,
            IOCP_CLIENT_CONTEXT::HEAD,
            context->client,
            new Message(CLIENT_SETUP)
        );
        wctx->transfer_message->set_body_json(client_setup);
        // sends the client setup message to the client if a successful setup is completed

        IOServerHelper::set_context_for_message(wctx);
        WSASend(context->client, &wctx->buffer, 1, nullptr, 0, &wctx->overlapped, nullptr);

        break;
    }
    case MessageType::CLIENT_TEXT_MESSAGE:

        context->transfer_message->set_sender(context->name);

        // on recieving a text message from client, message is broadcasted to every other client.
        std::cout << context->transfer_message->get_sender() << ": " << context->transfer_message->get_body_str() << '\n';

        broadcast(context, *(context->transfer_message));
        break;

    case MessageType::CLIENT_COMMAND: // on recieving a command it calls the handle command function
    {
        nlohmann::json cmd_jsn = context->transfer_message->get_body_json();

        Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Recieved Command: ", cmd_jsn["command"].get<std::string>(), " From " , context->name);

        handle_command(cmd_jsn, context);

        break;
    }
    case MessageType::CLIENT_LEAVE:
    {

        // On disconnect, disconnect message is sent to client, to cleanly disconnect from server.
        Message* disc = new Message(DISCONNECT);
        disc->set_body_string("Client Request");
        
        Logger::get().log(LogLevel::INFO, LogModule::SERVER, context->name, " Disconnected");

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
        if (context->transfer_message->get_type() == MessageType::ERRORMSG) Logger::get().log(LogLevel::ERR, LogModule::NETWORK, "Error in Client Socket");
        info.set_type(CLIENT_LEAVE);
        info.set_body_string(context->name);
        broadcast(context, info);
        context->room->remove_member(context->client);
        break;
    }
    }
}

void IOServerHelper::handle_command(nlohmann::json cmd_jsn, IOCP_CLIENT_CONTEXT *context) { // parses the command arguments
    std::string command = cmd_jsn["command"].get<std::string>();
    if (command == "subroom") { // / subroom
        if (!cmd_jsn["arg_num"].get<int>()) { // / no proper arguments provided, a help message is sent
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

            IOServerHelper::set_context_for_message(wctx);
            WSASend(context->client, &wctx->buffer, 1, nullptr, 0, &wctx->overlapped, nullptr);
            return;
        }
        std::vector<std::string> args = cmd_jsn["arguments"].get<std::vector<std::string>>();
        if (args[0] == "join") { // /subroom join <room_name>
            Message info = Message(CLIENT_LEAVE_ROOM); // adds the client to the room name subroom
            nlohmann::json data;
            data["client"] = context->name;
            data["room_name"] = args[1];
            info.set_body_json(data);
            info.set_sender("Server");

            broadcast(context, info);
            // tells every client in the current room that this user is leaving

            context->room = context->room->move_member_to_subroom(
                args[1],
                context->client
            ); // moves the user to the subroom

            info.set_type(CLIENT_JOIN_ROOM);
            info.set_body_string(context->name);
            info.set_sender("Server");

            // tells every user in the subroom that the client has joined
            broadcast(context, info);
        }
        else if (args[0] == "list") { // /subroom list
            Message info = Message(SERVER_MESSAGE); // lists all existing subrooms
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
            IOServerHelper::set_context_for_message(wctx);

            WSASend(context->client, &wctx->buffer, 1, nullptr, 0, &wctx->overlapped, nullptr);
        }
    }
}