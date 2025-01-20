#ifndef SERVER_HPP
#define SERVER_HPP

#include "server.hpp"
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <thread>
#include <vector>

class Server
{
public:
	Server();
	int create_tcp_socket(const char* port);

	int check(int result, std::string oper);

	addrinfo hints;
protected:
	SOCKET _listen_socket;
};

class ThreadedServer : public Server
{
public:
	ThreadedServer();

	int listen_and_accept(int max_connections);
private:
	std::vector<SOCKET> _connected_clients;
	int _client_thread(SOCKET client_socket, std::vector<SOCKET>& connected_clients);
};
#endif