#ifndef WMNET_CLIENT_HPP
#define WMNET_CLIENT_HPP

#include <atomic>
#include <functional>
#include <map>
#include <vector>
#include <thread>

#include "net/internals.hpp"
#include "net/tsqueue.hpp"
#include "net/message.hpp"

namespace WMNet {

	struct ServerMessage {
		NetHeader header;
		std::vector<byte> data;
	};

	class Client {
		using MessageHandler = std::function<void(ServerMessage)>;

	private:
		std::atomic<bool> connected;

		std::mutex queue_mutex;
		std::condition_variable queue_status;
		ThreadSafeQueue<ServerMessage> incoming_messages;

		std::mutex write_mutex;
		std::condition_variable write_status;
		ThreadSafeQueue<std::vector<byte>> outgoing_messages;

		SOCKET client_socket;
		
		std::map<msg_id_t, std::vector<MessageHandler>> message_handlers;

		int create_tcp_socket(const char* ip, const char* port);
		
		void send_loop();
		void read_loop();

		void message_listener();

	protected:
		Client();

		std::thread m_read_thread;
		std::thread m_write_thread;
		std::thread m_message_thread;

		void register_handler(msg_id_t id, MessageHandler handler);

		virtual void on_connect_to_server() {};

		template <class T>
		void send_message(Message<T> message) {
			std::vector<byte> raw_data = message.serialize();
			std::unique_lock<std::mutex> ulock(write_mutex);
			outgoing_messages.push(raw_data);

			write_status.notify_one();
		}
	public:
		bool is_connected();
		int start_client(const char* ip, const char* port);
		void stop_client();
	};

}

#endif // !WMNET_CLIENT_HPP