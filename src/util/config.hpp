#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "net/wmnet.hpp"

enum MessageIDs : WMNet::msg_id_t {
	
	ID_RequestLogin = WMNet::BeginCustom,
	ID_ClientLogin,
	ID_ClientJoined,
	ID_ClientLeave,
	ID_ClientMessage

};
enum CustomMessageTypes : WMNet::msg_type_t {
	MT_ClientMessage = WMNet::MessageType_Custom,
};

#endif