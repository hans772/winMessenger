#include "server/server.hpp"
#include "server/payloads.hpp"

void ChatRoom::remove_client(int id) {
	clients.erase(id);
}

void ChatRoom::add_client(std::shared_ptr<ServerClient> client) {
	clients[client->id] = client;
}

ChatServer::ChatServer() : lobby(std::make_shared<ChatRoom>(ChatRoom{"lobby"})) {

	register_handler(ID_ClientLogin, std::bind(&ChatServer::client_login, this, std::placeholders::_1));
	register_handler(ID_ClientMessage, std::bind(&ChatServer::client_message, this, std::placeholders::_1));
}

void ChatServer::on_connection_accepted(std::shared_ptr <WMNet::Connection> conn) {
	clients[conn->id()] = std::make_shared<ServerClient>(ServerClient{.id=conn->id(), .connection = conn});

	conn->send(WMNet::Message<WMNet::Empty>{.data = {}, .id = ID_RequestLogin });
}

void ChatServer::on_connection_dropped(std::shared_ptr <WMNet::Connection> conn) {
	
	auto v = clients.find(conn->id());
	if (v != clients.end()) {
		if (auto room = v->second->room.lock()) {
			room->remove_client(v->first);
		}
		clients.erase(v);
	}
}

void ChatServer::client_login(WMNet::ConnectionMessage msg) {
	auto v = clients.find(msg.conn->id());
	if (v == clients.end()) return;
	std::string name = WMNet::DeSerializer<std::string>::deserialize(msg.data);
	v->second->name = name;

	lobby->add_client(v->second);
	lobby->broadcast<std::string>({.data = name, .id = ID_ClientJoined}, { msg.conn->id() });
}

void ChatServer::client_message(WMNet::ConnectionMessage msg) {
	std::string message = WMNet::DeSerializer<std::string>::deserialize(msg.data);
	auto v = clients.find(msg.conn->id());
	if (v == clients.end()) return;
	if (auto room = v->second->room.lock()) {
		room->broadcast<ClientMessage>(
			{ 
				.data = {
					.client_id = v->second->id, 
					.client_name = v->second->name, 
					.message = message
				}, 
			.id = ID_ClientMessage
		}, { v->second->id });
	}
	clients.erase(v);


}
