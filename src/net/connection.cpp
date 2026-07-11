
#include "net/connection.hpp"

namespace WMNet {

	Connection::Connection(size_t id, SOCKET sock, WMNetMessageCallback callback) : 
		conn_id(id), 
		socket(sock), 
		r_progress(0),
		w_progress(0), 
		message_callback(callback),
		active_contexts(0), 
		is_disconnecting(false) 
	{}

	int Connection::id() const {
		return conn_id;
	}

	void Connection::try_send(ULONG flags) {
		int res = WSASend(socket, &write_context.wsa_buf, 1, nullptr, flags, &write_context.overlapped, nullptr);
		active_contexts++;

		if (res == SOCKET_ERROR) {
			int err = WSAGetLastError();
			if (err != WSA_IO_PENDING) {
				std::vector<byte> v;
				message_callback(shared_from_this(), { ConnectionDropped, MessageType_Empty, 0 }, v);

				write_context.in_service.store(false);
			}
		}
	}

	void Connection::try_receive(ULONG flags) {
		read_context.flags = flags;
		int res = WSARecv(socket, &read_context.wsa_buf, 1, nullptr, &read_context.flags, &read_context.overlapped, nullptr);
		active_contexts++;

		if (res == SOCKET_ERROR) {
			int err = WSAGetLastError();
			if (err != WSA_IO_PENDING) {

				std::vector<byte> v;
				message_callback(shared_from_this(), { ConnectionDropped, MessageType_Empty, 0 }, v);

				read_context.in_service.store(false);
			}
		}
	}

	void Connection::begin_send() {
			
		if (write_context.in_service.exchange(true)) return;
		if (!network_queue.try_pop(current_write)) {
			write_context.in_service.store(false);
			return;
		}

		write_context.set_data(current_write);
		w_progress = 0u;
		try_send(0);
	}

	void Connection::on_write_completed() {

		size_t bytes_sent = write_context.completed_bytes;

		if (w_progress+bytes_sent < current_write.size()) {
			w_progress += bytes_sent;
			write_context.set_data(current_write, w_progress);
			try_send(0);
			return;
		}

		write_context.in_service.store(false);
		begin_send();
	}

	void Connection::begin_read() {
		
		if (read_context.in_service.exchange(true)) return;
		read_context.part = IO_Part::HEADER;
		read_context.set_data(reinterpret_cast<byte *>(&curr_read_header), sizeof(NetHeader));
		
		try_receive(0);

	}


	void Connection::on_read_completed() {

		if (read_context.part == IO_Part::HEADER) {
			curr_read_header.id = ntohs(curr_read_header.id);
			curr_read_header.type = ntohs(curr_read_header.type);
			curr_read_header.size = ntohs(curr_read_header.size);

			if (!curr_read_header.size) {
				curr_read_data.clear();
				message_callback(shared_from_this(), curr_read_header, curr_read_data);
				read_context.in_service.store(false);
				begin_read();
				return;
			}

			curr_read_data.resize(curr_read_header.size);
			read_context.part = IO_Part::BODY;
			r_progress = 0u;
			read_context.set_data(curr_read_data);

			try_receive(0);
			return;
		}
		else if (read_context.part == IO_Part::BODY) {
			size_t bytes_received = read_context.completed_bytes;

			if (r_progress + bytes_received < curr_read_header.size) {
				r_progress += bytes_received;
				read_context.set_data(curr_read_data, r_progress);
				try_receive(0);
				return;
			}
			
			message_callback(shared_from_this(), curr_read_header, curr_read_data);
			read_context.in_service.store(false);
			begin_read();
		}
	}
}