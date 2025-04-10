#include <mutex>
#include <WinSock2.h>

#include "rooms.hpp"

ChatRoom::ChatRoom(std::string room_name) : room_mutex(std::make_shared<std::mutex>()) { name = room_name; }

ChatRoom::ChatRoom(const ChatRoom& other) : name(other.name), members(other.members), sub_rooms(other.sub_rooms), room_mutex(other.room_mutex) {}

void ChatRoom::add_member(SOCKET member) {

	std::lock_guard<std::mutex>	lock(*room_mutex);
	members.insert(member);

}

void ChatRoom::remove_member(SOCKET member) {

	std::lock_guard<std::mutex>	lock(*room_mutex);
	members.erase(member);
}

SubRoom::SubRoom(ChatRoom* parent_room, const std::string& name) : ChatRoom(name), parent(parent_room) {};

SubRoom::SubRoom(const SubRoom& other) : ChatRoom(other), parent(other.parent) {};

void SubRoom::move_up(SOCKET member) {
	parent->add_member(member);
	remove_member(member);
}