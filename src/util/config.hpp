#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "net/wmnet.hpp"

namespace Color {
	constexpr const char* RESET = "\033[0m";
	constexpr const char* RED = "\033[31m";
	constexpr const char* GREEN = "\033[32m";
	constexpr const char* YELLOW = "\033[33m";
	constexpr const char* BLUE = "\033[34m";
	constexpr const char* MAGENTA = "\033[35m";
	constexpr const char* CYAN = "\033[36m";
	constexpr const char* WHITE = "\033[37m";
	constexpr const char* BOLD = "\033[1m";
}

enum STARTUP_ITEM : int {
	SERVER = 0,
	CLIENT = 1
};

enum MessageIDs : WMNet::msg_id_t {
	
	ID_RequestLogin = WMNet::BeginCustom,

	ID_ServerMessage,

	ID_ClientLogin,
	ID_AcknowledgeLogin,

	ID_ClientMessage,

	ID_CreateSubroom,
	ID_JoinSubroom,
	ID_BackToParent,
};
enum CustomMessageTypes : WMNet::msg_type_t {
	MT_ClientMessage = WMNet::MessageType_Custom,
};

#endif