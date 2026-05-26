#ifndef WMNET_IOCP_HPP
#define WMNET_IOCP_HPP

#include <WinSock2.h>
#include <vector>
#include <atomic>

#include "net/internals.hpp"

namespace WMNet {

	struct IOCP_CONTEXT {

		inline static size_t BUFFER_SIZE = 8192;

		OVERLAPPED overlapped;
		WSABUF wsa_buf;

		DWORD flags;

		std::atomic<bool> in_service;

		size_t completed_bytes;
		
		IO_Oper operation;
		IO_Part part;

		IOCP_CONTEXT(IO_Oper op, IO_Part part);

		static void set_max_buffer_size(size_t size);
		ULONG set_data(std::vector<byte>& data, size_t offset = 0);
		ULONG set_data(byte *data, size_t size);
	};
}

#endif // !WMNET_IOCP_HPP
