#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <map>
#include <algorithm>
#include <memory>

#include "util/config.hpp"
#include "net/wmnet.hpp"

struct ChatRoom ;

struct ServerClient {
	bool logged_in;
	int id;
	std::string name;
	std::shared_ptr<WMNet::Connection> connection;
	std::weak_ptr<ChatRoom> room;
};

struct ChatRoom : public std::enable_shared_from_this<ChatRoom> {
	std::string name;
	std::map <int, std::shared_ptr<ServerClient>> clients;
	std::map <int, std::shared_ptr<ChatRoom>> sub_rooms;

	ChatRoom(std::string name);

	void remove_client(int id);
	void add_client(std::shared_ptr<ServerClient> client);

	template <typename T>
	void broadcast(WMNet::Message<T> msg, std::vector<int> ignores) {
		std::sort(ignores.begin(), ignores.end());

		int n = ignores.size();
		int i = 0;
		for (auto& client : clients) {
			while (i < n && client.second->id > ignores[i]) i++;
			if (i < n && client.second->id == ignores[i]) {
				i++;
				continue;
			}
			client.second->connection->send(msg);
		}
	}
};



class ChatServer : public WMNet::Server {

	std::shared_ptr<ChatRoom> lobby;
	std::map <int, std::shared_ptr<ServerClient>> clients;

	void on_connection_accepted(std::shared_ptr <WMNet::Connection> conn) override;
	void on_connection_dropped(std::shared_ptr <WMNet::Connection> conn) override;
	
	void client_login(WMNet::ConnectionMessage msg);
	void client_message(WMNet::ConnectionMessage msg);

public:
	ChatServer();

};

#endif