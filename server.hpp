#ifndef SERVER_HPP
#define SERVER_HPP

#include <WinSock2.h>
#include <vector>
#include "rooms.hpp"
#include "server_client.hpp"
#include "io_server_helper.hpp"
#include <MSWSock.h>
#include <Windows.h>
#include "mutex"

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
	std::vector<std::shared_ptr<ChatRoom>> chat_rooms;
};

class IOServer : public Server
{
public:
	IOServer();
	HANDLE iocp;
	std::shared_ptr<std::mutex> crooms_mutex;
	int listen_and_accept(int max_connections);
	static DWORD WINAPI worker_thread(LPVOID lpParam);
private:
	std::vector<std::shared_ptr<ChatRoom>> chat_rooms;
};
#endif