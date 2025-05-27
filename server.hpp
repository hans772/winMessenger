#ifndef SERVER_HPP
#define SERVER_HPP

#include <WinSock2.h>
#include <vector>
#include "server_client.hpp"
#include "rooms.hpp"
#include <MSWSock.h>
#include <Windows.h>

struct IOCP_CLIENT_CONTEXT {
	OVERLAPPED overlapped;
	WSABUF buffer;
	int expected_bytes;
	int recieved_bytes;
	char* transfer_data;
	std::shared_ptr<ServerClient> client;
	enum {READ, WRITE} operation;
	enum {HEAD, HEADER_JS, BODY} part;

	Message *transfer_message;
};

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
	HANDLE iocp;
	int listen_and_accept(int max_connections);
	static DWORD WINAPI worker_thread(LPVOID lpParam);
private:
	std::vector<std::shared_ptr<ChatRoom>> chat_rooms;
};
#endif