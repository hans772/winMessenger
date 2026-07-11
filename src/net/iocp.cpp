#include "net/iocp.hpp"
#include <algorithm>

namespace WMNet {

	IOCP_CONTEXT::IOCP_CONTEXT(IO_Oper op, IO_Part part) :
		operation(op),
		part(part),
		in_service(false),
		flags(0),
		completed_bytes(0)
	{
		ZeroMemory(&overlapped, sizeof(WSAOVERLAPPED));
		wsa_buf.buf = nullptr;
		wsa_buf.len = 0;
	}

	void IOCP_CONTEXT::set_max_buffer_size(size_t size) {
		IOCP_CONTEXT::BUFFER_SIZE = size;
	}

	ULONG IOCP_CONTEXT::set_data(std::vector<byte>& data, size_t offset) {
		size_t bytes = min(IOCP_CONTEXT::BUFFER_SIZE, data.size() - offset);
		wsa_buf.buf = reinterpret_cast<char*>(data.data() + offset);
		completed_bytes = 0;
		return wsa_buf.len = static_cast<ULONG>(bytes);
	}

	ULONG IOCP_CONTEXT::set_data(byte *data, size_t size) {
		size_t bytes = min(IOCP_CONTEXT::BUFFER_SIZE, size);
		wsa_buf.buf = reinterpret_cast<char*>(data);
		completed_bytes = 0;
		return wsa_buf.len = static_cast<ULONG>(bytes);
	}

}