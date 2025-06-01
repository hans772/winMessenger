#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <WinSock2.h>
#include <vector>
#include <string>
#include "auth.hpp"

class Client
{
public:
	Client();
	int create_tcp_socket(const char* ip, const char* port, std::string ip_str);
	std::string input;
	int check(int result, std::string oper);
	std::string connected_ip;

	ClientAuth auth;
	addrinfo hints;
protected:
	std::string joined_room;
	int _connected;
	SOCKET _connect_socket;
};

class ThreadedClient : public Client
{
public:
	ThreadedClient();

	int start_chat();
private:
	int client_verified;

	void _restore_client_input();
	void _clear_console_line();
	int _listen_thread(SOCKET server);
	int _write_thread(SOCKET server);
};
#endif