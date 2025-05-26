#ifndef SERVER_CLIENT_HPP
#define SERVER_CLIENT_HPP

#include "rooms.hpp"
#include <WinSock2.h>
#include "json.hpp"
#include "message.hpp"

class ServerClient
{
private:
	std::shared_ptr<ChatRoom> current_room;
	SOCKET client_socket;
public:
	std::string client_name;
	ServerClient(std::shared_ptr<ChatRoom> room, SOCKET socket, std::string name);
	void broadcast(Message msg);
	void client_thread();
	void handle_command(nlohmann::json cmd_json);
};

#endif // !SERVER_CLIENT_HPP
