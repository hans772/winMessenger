#ifndef WMNET_INTERNALS_HPP
#define WMNET_INTERNALS_HPP

#include <string>

class NotImplementedException : public std::exception {
private:
	std::string message;
public:
	NotImplementedException(const char* msg = "Functionality not yet implemented!") : message(msg) {}

	virtual const char* what() const noexcept override {
		return message.c_str();
	}
};

namespace WMNet {
	typedef char byte;
	typedef std::uint16_t msg_id_t;
	typedef std::uint16_t msg_type_t;
	typedef std::uint16_t msg_size_t;


	template <typename>
	inline constexpr bool always_false_v = false;

	#pragma pack(push, 1)
	struct NetHeader {
		msg_id_t id;
		msg_type_t type;
		msg_size_t size;
	};
	#pragma pack(pop)

	enum IO_Oper { READ, WRITE };
	enum IO_Part { HEADER, BODY, UNKNOWN };
	
	enum SerializableTypes : msg_type_t {
		MessageType_UInt32 = 0,
		MessageType_UInt64 = 1,
		MessageType_String = 2,
		MessageType_JSON = 3,
		MessageType_Empty = 4,
		MessageType_Custom = 10
	};

	enum ReservedMessages : msg_id_t {
		Unknown = 0,
		Ping = 1,
		ConnectionAccepted = 2,
		ConnectionDropped = 3,
		BeginCustom = 10
	};

	struct Empty {};
}



#endif // !WMNET_INTERNALS_HPP
