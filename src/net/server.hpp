#ifndef WMNET_SERVER_HPP
#define WMNET_SERVER_HPP

#include <WinSock2.h>
#include <MSWSock.h>
#include <Windows.h>
#include <map>
#include <memory>
#include <shared_mutex>

#include "net/connection.hpp"

namespace WMNet {

	struct ConnectionMessage {
		std::shared_ptr<Connection> conn;
		NetHeader header;
		std::vector<byte> data;
	};

	enum class NetEvent {
		ConnectionAccept,
		ConnectionDropped,
		SafeToDestroy
	};

	class Server {

		using MessageHandler = std::function<void(ConnectionMessage)>;
		using EventHandler = std::function<void(std::shared_ptr<Connection>)>;

	private:
		static DWORD WINAPI worker_thread(LPVOID lpParam);
		std::atomic<bool> active;

		std::shared_mutex conn_mtx;
		std::map<int, std::shared_ptr<Connection>> connections;
		std::map<int, std::shared_ptr<Connection>> disconnections;

		ThreadSafeQueue<ConnectionMessage> incoming_queue;
		std::condition_variable queue_status;
		std::mutex queue_mutex;
		void listen_connections();

		std::map<msg_id_t, std::vector<MessageHandler>> message_handlers;
		void push_message(std::shared_ptr<Connection> conn, NetHeader header, std::vector<byte>& data);

		ThreadSafeQueue<std::pair<std::shared_ptr<Connection>, NetEvent>> event_queue;
		std::condition_variable event_status;
		std::mutex event_mutex;
		void listen_events();
		void push_event(std::shared_ptr<Connection> conn, NetEvent event);

		void ping(ConnectionMessage inc_m);

		void connection_accept(std::shared_ptr<Connection> conn);
		void connection_drop(std::shared_ptr<Connection> conn);
		void connection_destroy(std::shared_ptr<Connection> conn);

		int create_tcp_socket(const char* port);
		void accept_connections(int max_connections);
		int close_server();

		SOCKET listen_socket;
		HANDLE ioc_port;

	protected:

		std::thread m_acceptor_thread;
		std::thread m_message_thread;
		std::thread m_event_thread;

		// This is for developers to not worry about concurrency
		// Ensure that no blocking functions are run on any of the event / message callbacks
		// Do not spawn threads which handle server state without locking this mutex

		std::mutex app_mutex; 

		virtual void on_connection_dropped(std::shared_ptr<Connection> conn) {}
		virtual void on_connection_accepted(std::shared_ptr<Connection> conn) {}
		virtual void on_connection_destroyed(std::shared_ptr<Connection> conn) {}

		Server();
		void register_handler(msg_id_t id, MessageHandler handler);
		template <class T>
		void send_all(const Message<T>& msg) {
			std::shared_lock<std::shared_mutex> lock(conn_mtx);

			for (auto& conn : connections) {
				conn.second->send<T>(msg);
			}
		};
	public:
		int start_server(const char* port, size_t max_connections);
		void stop_server();
	};

}

#endif // !WMNET_SERVER_HPP
