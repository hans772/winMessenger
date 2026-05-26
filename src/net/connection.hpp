#ifndef WMNET_CONNECTION_HPP
#define WMNET_CONNECTION_HPP

#include <cstdint>
#include <WinSock2.h>
#include <vector>
#include <functional>
#include <memory>

#include "net/internals.hpp"
#include "net/tsqueue.hpp"
#include "net/iocp.hpp"
#include "net/message.hpp"

namespace WMNet {
	class Connection;

	using WMNetMessageCallback = std::function<void(std::shared_ptr<Connection>, NetHeader, std::vector<byte>&)>;

	class Connection: public std::enable_shared_from_this<Connection> {

		friend class Server;
		
		SOCKET socket;
		int conn_id;
		
		ThreadSafeQueue<std::vector<byte>> network_queue;
		
		IOCP_CONTEXT read_context{ WMNet::READ, WMNet::UNKNOWN };
		std::vector<byte> curr_read_data;
		NetHeader curr_read_header;
		size_t r_progress;

		WMNetMessageCallback message_callback;

		IOCP_CONTEXT write_context{ WMNet::WRITE, WMNet::UNKNOWN };
		std::vector<byte> current_write;
		size_t w_progress;

		std::atomic<size_t> active_contexts;
		std::atomic<bool> is_disconnecting;

		void try_send(ULONG flags = 0);
		void try_receive(ULONG flags = 0);
		
		void on_write_completed();
		void on_read_completed();

		void begin_read();
		void begin_send();

	public:
		
		
		Connection(size_t id, SOCKET sock, WMNetMessageCallback callback);

		int id() const;

		template <class T>
		void send(const Message<T>& msg) {
			std::vector<byte> raw_data = msg.serialize();
			network_queue.push(raw_data);

			begin_send();
		};
	};
}


#endif // !WMNET_CONNECTION_HPP
