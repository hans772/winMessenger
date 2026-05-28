#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "net/wmnet.hpp"

enum STARTUP_ITEM : int {
	SERVER = 0,
	CLIENT = 1
};

enum MessageIDs : WMNet::msg_id_t {
	
	ID_RequestLogin = WMNet::BeginCustom,
	ID_ClientLogin,
	ID_AcknowledgeLogin,
	ID_ClientJoined,
	ID_ClientLeave,
	ID_ClientMessage

};
enum CustomMessageTypes : WMNet::msg_type_t {
	MT_ClientMessage = WMNet::MessageType_Custom,
};

#endif