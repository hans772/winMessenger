#pragma once

#include "message.hpp"
#include <MSWSock.h>
#include <Windows.h>
#include <WinSock2.h>
#include "json.hpp"
#include "rooms.hpp"
#include "message.hpp"
#include <mutex>
#include "auth.hpp"


// every asynchronous operation is done with a context, it requires only an OVERLAPPED and WSABUF but other parameters are added for convenience

struct IOCP_CLIENT_CONTEXT {

	enum Operation { READ, WRITE };
	enum Part { HEAD, HEADER_JS, BODY };

	// one overlapped per operation
	OVERLAPPED overlapped;
	WSABUF buffer;
	int expected_bytes;
	int recieved_bytes;
	char* transfer_data;
	SOCKET client; // the socket on which the operation was just completed
	std::shared_ptr<ChatRoom> room; // shared pointer to the chatroom the user joins
	std::string name; // name of the user
	Operation operation; // operation just completed
	Part part; // part of the message which was just sent

	Message* transfer_message; // message transferred or to be transferred.
};

class IOServerHelper {
	std::vector<std::shared_ptr<ChatRoom>> *server_rooms;
	std::shared_ptr<std::mutex> server_mutex;
	std::shared_ptr<ServerAuth> auth;
public:
	IOServerHelper(std::vector<std::shared_ptr<ChatRoom>> *rooms, std::shared_ptr<std::mutex> mutex, std::shared_ptr<ServerAuth> auth);
	void handle_read(IOCP_CLIENT_CONTEXT* context);
	void handle_write(IOCP_CLIENT_CONTEXT* context);
	void handle_message(IOCP_CLIENT_CONTEXT* context);
	void handle_command(nlohmann::json cmd_jsn, IOCP_CLIENT_CONTEXT *context);
	void broadcast(IOCP_CLIENT_CONTEXT* context, Message message); // broadcast to all clients in current room
	static IOCP_CLIENT_CONTEXT* create_new_context(IOCP_CLIENT_CONTEXT::Operation oper,
		IOCP_CLIENT_CONTEXT::Part part,
		SOCKET socket,
		Message* message);
	static void set_context_for_message(IOCP_CLIENT_CONTEXT* context); // prepares a message for writing to socket
};