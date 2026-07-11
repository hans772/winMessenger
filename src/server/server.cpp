#include "server/server.hpp"
#include "util/payloads.hpp"
#include "util/logger.hpp"

ChatRoom::ChatRoom(std::string name) : name(name) {}

ChatRoom::ChatRoom(std::string name, int id, std::shared_ptr<ChatRoom> parent): name(name), id(id), parent(parent) {}

void ChatRoom::remove_client(int id) {
	clients.erase(id);
}

void ChatRoom::add_client(std::shared_ptr<ServerClient> client) {
	clients[client->id] = client;
	client->room = shared_from_this();
}

std::shared_ptr<ChatRoom> ChatRoom::create_subroom(std::string name) {
	int id = sub_rooms.empty() ? 0 : sub_rooms.rbegin()->first + 1;
	return sub_rooms[id] = std::make_shared<ChatRoom>(name, id, shared_from_this());

}

std::shared_ptr<ChatRoom> ChatRoom::find_subroom(int id) {
	auto sr = sub_rooms.find(id);

	if (sr == sub_rooms.end()) return sr->second;

	return nullptr;
}

std::shared_ptr<ChatRoom> ChatRoom::find_subroom(std::string name) {

	for (auto it = sub_rooms.begin(); it != sub_rooms.end(); it++) {
		if (it->second->name == name) return it->second;
	}
	return nullptr;
}

bool ChatRoom::move_client_to_subroom(int id, std::shared_ptr<ServerClient> client) {
	auto sr = sub_rooms.find(id);

	if (sr == sub_rooms.end()) return false;

	if (!clients.erase(client->id)) return false;;
	sr->second->add_client(client);

	client->room = sr->second;

	return true;
}

bool ChatRoom::move_client_to_parent(std::shared_ptr<ServerClient> client) {
	if (auto par = parent.lock()) {
		if (!clients.erase(client->id)) return false;
		par->add_client(client);
		client->room = par;
		return true;
	}

	return false;

}

ChatServer::ChatServer() : lobby(std::make_shared<ChatRoom>("lobby")) {

	register_handler(ID_ClientLogin, std::bind(&ChatServer::client_login, this, std::placeholders::_1));
	register_handler(ID_ClientMessage, std::bind(&ChatServer::client_message, this, std::placeholders::_1));
	register_handler(ID_CreateSubroom, std::bind(&ChatServer::client_create_subroom, this, std::placeholders::_1));
	register_handler(ID_JoinSubroom, std::bind(&ChatServer::client_join_subroom, this, std::placeholders::_1));
	register_handler(ID_BackToParent, std::bind(&ChatServer::client_parent_subroom, this, std::placeholders::_1));
}

void ChatServer::on_connection_accepted(std::shared_ptr <WMNet::Connection> conn) {
	clients[conn->id()] = std::make_shared<ServerClient>(ServerClient{ .logged_in = false, .id=conn->id(), .connection = conn });

	Logger::get().log(LogLevel::DEBUG, LogModule::SERVER, "Client ", conn->id(), " Connected");


	conn->send(WMNet::Message<WMNet::Empty>{.data = {}, .id = ID_RequestLogin });
}

void ChatServer::on_connection_dropped(std::shared_ptr <WMNet::Connection> conn) {
	
	Logger::get().log(LogLevel::DEBUG, LogModule::SERVER, "Client ", conn->id(), " Disconnected.");


	auto v = clients.find(conn->id());
	if (v != clients.end()) {
		if (auto room = v->second->room.lock()) {
			room->remove_client(v->first);
			lobby->broadcast<std::string>({ .data = v->second->name + " left the server", .id = ID_ServerMessage}, {conn->id()});
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
	Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Client ", msg.conn->id(), " logged in with name: ", name);

	msg.conn->send(WMNet::Message<WMNet::Empty>{.id = ID_AcknowledgeLogin});
	lobby->add_client(v->second);
	lobby->broadcast<std::string>({.data = name + " joined the server", .id = ID_ServerMessage}, {msg.conn->id()});
}

void ChatServer::client_message(WMNet::ConnectionMessage msg) {

	std::string message = WMNet::DeSerializer<std::string>::deserialize(msg.data);
	Logger::get().log(LogLevel::INFO, LogModule::SERVER, "Received message from: ", msg.conn->id());


	auto v = clients.find(msg.conn->id());
	if (v == clients.end()) return;
	if (!(v->second->logged_in)) {
		msg.conn->send(WMNet::Message<WMNet::Empty>{.data = {}, .id = ID_RequestLogin });
		return;
	};

	if (auto room = v->second->room.lock()) {
		room->broadcast<ClientString>(
			{ 
				.data = {
					.client_id = v->second->id, 
					.client_name = v->second->name, 
					.str = message
				}, 
			.id = ID_ClientMessage
		}, { v->second->id });
	}
}

void ChatServer::client_create_subroom(WMNet::ConnectionMessage msg) {
	std::string subroom_name = WMNet::DeSerializer<std::string>::deserialize(msg.data);

	auto v = clients.find(msg.conn->id());
	if (v == clients.end()) return;
	if (!(v->second->logged_in)) {
		msg.conn->send(WMNet::Message<WMNet::Empty>{.data = {}, .id = ID_RequestLogin });
		return;
	};

	if (auto room = v->second->room.lock()) {
		auto subr = room->create_subroom(subroom_name);
		room->move_client_to_subroom(subr->id, v->second);
		room->broadcast<std::string>(
			{
			.data= v->second->name + " just left to room: " + subroom_name,
			.id = ID_ServerMessage
			}, { v->second->id });

		msg.conn->send(WMNet::Message<std::string>{.data = "successfully joined room: " + subroom_name, .id = ID_ServerMessage});
		subr->broadcast<std::string>({ .data = v->second->name + " just joined the room", .id = ID_ServerMessage}, {msg.conn->id()});
	}

}

void ChatServer::client_join_subroom(WMNet::ConnectionMessage msg) {
	

	auto v = clients.find(msg.conn->id());
	if (v == clients.end()) return;
	if (!(v->second->logged_in)) {
		msg.conn->send(WMNet::Message<WMNet::Empty>{.data = {}, .id = ID_RequestLogin });
		return;
	};

	if (auto room = v->second->room.lock()) {
		std::shared_ptr<ChatRoom> subr;

		if (msg.header.type == WMNet::MessageType_String) {
			std::string subroom_name = WMNet::DeSerializer<std::string>::deserialize(msg.data);
			subr = room->find_subroom(subroom_name);

		}
		else if (msg.header.type == WMNet::MessageType_UInt32) {
			int id = WMNet::DeSerializer<uint32_t>::deserialize(msg.data);
			subr = room->find_subroom(id);
		}

		if (!subr) {
			msg.conn->send(WMNet::Message<std::string>{.data = "unable to join room", .id = ID_ServerMessage});
			return;
		}

		room->move_client_to_subroom(subr->id, v->second);
		room->broadcast<std::string>(
			{
			.data = v->second->name + " just left to room: " + subr->name,
			.id = ID_ServerMessage
			}, { v->second->id });

		msg.conn->send(WMNet::Message<std::string>{.data = "successfully joined room: " + subr->name, .id = ID_ServerMessage});
		subr->broadcast<std::string>({ .data = v->second->name + " just joined the room", .id = ID_ServerMessage }, { msg.conn->id() });
	}

}

void ChatServer::client_parent_subroom(WMNet::ConnectionMessage msg) {
	auto v = clients.find(msg.conn->id());
	if (v == clients.end()) return;
	if (!(v->second->logged_in)) {
		msg.conn->send(WMNet::Message<WMNet::Empty>{.data = {}, .id = ID_RequestLogin });
		return;
	};

	if (auto room = v->second->room.lock()) {
		if (!room->move_client_to_parent(v->second)) {
			msg.conn->send(WMNet::Message<std::string>{.data = "unable to join room", .id = ID_ServerMessage});
			return;
		};
		room->broadcast<std::string>(
			{
			.data = v->second->name + " just left to the parent room",
			.id = ID_ServerMessage
			}, { v->second->id });

		if (auto par = v->second->room.lock()) {
			msg.conn->send(WMNet::Message<std::string>{.data = "successfully joined parent room: " + par->name, .id = ID_ServerMessage});
			par->broadcast<std::string>({ .data = v->second->name + " just joined the room", .id = ID_ServerMessage }, { msg.conn->id() });
		}
	}
}
