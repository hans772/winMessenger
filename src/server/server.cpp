#include "server/server.hpp"
#include "util/payloads.hpp"
#include "util/logger.hpp"

ChatRoom::ChatRoom(std::string name) {
	this->name = name;
}

void ChatRoom::remove_client(int id) {
	clients.erase(id);
}

void ChatRoom::add_client(std::shared_ptr<ServerClient> client) {
	clients[client->id] = client;
	client->room = shared_from_this();
}

ChatServer::ChatServer() : lobby(std::make_shared<ChatRoom>("lobby")) {

	register_handler(ID_ClientLogin, std::bind(&ChatServer::client_login, this, std::placeholders::_1));
	register_handler(ID_ClientMessage, std::bind(&ChatServer::client_message, this, std::placeholders::_1));
}

void ChatServer::on_connection_accepted(std::shared_ptr <WMNet::Connection> conn) {
	clients[conn->id()] = std::make_shared<ServerClient>(ServerClient{ .logged_in = false, .id=conn->id(), .connection = conn });

	Logger::get().log(LogLevel::DEBUG, LogModule::SERVER, "Client ", conn->id(), " Connected");


	conn->send(WMNet::Message<WMNet::Empty>{.data = {}, .id = ID_RequestLogin });
}

void ChatServer::on_connection_dropped(std::shared_ptr <WMNet::Connection> conn) {
	
	Logger::get().log(LogLevel::DEBUG, LogModule::SERVER, "Client ", conn->id(), "Disconnected.");


	auto v = clients.find(conn->id());
	if (v != clients.end()) {
		if (auto room = v->second->room.lock()) {
			room->remove_client(v->first);
			lobby->broadcast<std::string>({ .data = v->second->name, .id = ID_ClientLeave }, { conn->id() });
		}
		clients.erase(v);
	}
}

void ChatServer::client_login(WMNet::ConnectionMessage msg) {
	auto v = clients.find(msg.conn->id());
	if (v == clients.end()) return;
	std::string name = WMNet::DeSerializer<std::string>::deserialize(msg.data);
	v->second->name = name;
	v->second->logged_in = true;
	Logger::get().log(LogLevel::DEBUG, LogModule::SERVER, "Client ", msg.conn->id(), " logged In.");

	msg.conn->send(WMNet::Message<WMNet::Empty>{.id = ID_AcknowledgeLogin});
	lobby->add_client(v->second);
	lobby->broadcast<std::string>({.data = name, .id = ID_ClientJoined}, { msg.conn->id() });
}

void ChatServer::client_message(WMNet::ConnectionMessage msg) {

	std::string message = WMNet::DeSerializer<std::string>::deserialize(msg.data);

	Logger::get().log(LogLevel::DEBUG, LogModule::SERVER, "Client ", msg.conn->id(), " sent message: ", message);


	auto v = clients.find(msg.conn->id());
	if (v == clients.end()) return;
	if (!(v->second->logged_in)) {
		msg.conn->send(WMNet::Message<WMNet::Empty>{.data = {}, .id = ID_RequestLogin });
		return;
	};

	if (auto room = v->second->room.lock()) {
		Logger::get().log(LogLevel::DEBUG, LogModule::SERVER, "Broadcasted ", msg.conn->id(), "'s message.");
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
}
