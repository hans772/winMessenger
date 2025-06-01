#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <iostream>
#include <map>
#include <WinSock2.h>
#include <string>
#include "json.hpp"

const int MSGBODY_STRING = 700;
const int MSGBODY_INT = 701;
const int MSGBODY_JSON = 702;

enum MessageType {
	DEFAULT_MSG,
	CLIENT_JOIN,
	CLIENT_LEAVE,
	CLIENT_TEXT_MESSAGE,
	SERVER_MESSAGE,
	ERRORMSG,
	DISCONNECT,
	CLIENT_COMMAND,
	CLIENT_JOIN_ROOM,
	CLIENT_LEAVE_ROOM,
	CLIENT_SETUP,
	CLIENT_INVALID_USERNAME,
	CLIENT_INVALID_TOKEN
};

static const std::map<std::string, const int> MESSAGE_TYPES = {
	{"client_join" , CLIENT_JOIN},
	{"client_leave", CLIENT_LEAVE},
	{"client_textmsg", CLIENT_TEXT_MESSAGE},
	{"server_message", SERVER_MESSAGE},
	{"error", ERRORMSG},
	{"disconnect", DISCONNECT},
	{"client_command", CLIENT_COMMAND}
};


class Message {

public:
	Message(MessageType type = MessageType::DEFAULT_MSG);
	Message(Message& other);

private:
	nlohmann::json headerjson;

	int body_type;

	std::string body_s;
	std::int32_t body_i;
	nlohmann::json body_json;

public:
	std::pair<std::string, int> serialize_header();
	static nlohmann::json deserialize_header(std::string msg);

	static Message recieve_message(SOCKET socket);
	int send_message(SOCKET socket);

	nlohmann::json get_header();
	std::string get_sender();
	int get_bodylen();
	int get_type();
	int get_body_type();

	void set_type(MessageType type);
	void set_body_type(int type);
	void set_sender(std::string sender);
	void set_body_string(std::string str);
	void set_body_int(int integ);
	void set_body_json(nlohmann::json json);

	void set_body_from_buffer(int type, char* bodybuf, int body_length);

	std::string get_body_str();
	int get_body_int();
	int32_t get_body_int32();
	nlohmann::json get_body_json();

	void print_message();
};

#endif