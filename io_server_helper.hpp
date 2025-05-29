#pragma once

#include "message.hpp"
#include <MSWSock.h>
#include <Windows.h>
#include <WinSock2.h>
#include "json.hpp"
#include "rooms.hpp"
#include "message.hpp"
#include <mutex>

struct IOCP_CLIENT_CONTEXT {

	enum Operation { READ, WRITE };
	enum Part { HEAD, HEADER_JS, BODY };

	OVERLAPPED overlapped;
	WSABUF buffer;
	int expected_bytes;
	int recieved_bytes;
	char* transfer_data;
	SOCKET client;
	std::shared_ptr<ChatRoom> room;
	std::string name;
	Operation operation;
	Part part;

	Message* transfer_message;
};

class IOServerHelper {
	std::vector<std::shared_ptr<ChatRoom>> *server_rooms;
	std::shared_ptr<std::mutex> server_mutex;
public:
	IOServerHelper(std::vector<std::shared_ptr<ChatRoom>> *rooms, std::shared_ptr<std::mutex> mutex);
	void handle_read(IOCP_CLIENT_CONTEXT* context);
	void handle_write(IOCP_CLIENT_CONTEXT* context);
	void handle_message(IOCP_CLIENT_CONTEXT* context);
	void handle_command(nlohmann::json cmd_jsn, IOCP_CLIENT_CONTEXT *context);
	void broadcast(IOCP_CLIENT_CONTEXT* context, Message message);
	static IOCP_CLIENT_CONTEXT* create_new_context(IOCP_CLIENT_CONTEXT::Operation oper,
		IOCP_CLIENT_CONTEXT::Part part,
		SOCKET socket,
		Message* message);
	static void set_context_for_message(IOCP_CLIENT_CONTEXT* context);
};