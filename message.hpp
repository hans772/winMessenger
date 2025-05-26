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

enum MessageTypes {
	DEFAULT_MSG = 0,
	CLIENT_JOIN = 400,
	CLIENT_LEAVE = 401,
	CLIENT_TEXT_MESSAGE = 402,
	SERVER_MESSAGE = 403,
	ERRORMSG = 404,
	DISCONNECT = 405,
	CLIENT_COMMAND = 406,
	CLIENT_JOIN_ROOM = 407,
	CLIENT_LEAVE_ROOM = 408
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
	Message(int type);

private:
	nlohmann::json headerjson;

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

	void set_type(int type);
	void set_sender(std::string sender);
	void set_body_string(std::string str);
	void set_body_int(int integ);
	void set_body_json(nlohmann::json json);

	std::string get_body_str();
	int get_body_int();
	nlohmann::json get_body_json();

	void print_message();
};

#endif