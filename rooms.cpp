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

std::shared_ptr <SubRoom> ChatRoom::move_member_to_subroom(std::string room_name, SOCKET member) {

	for (std::shared_ptr <SubRoom> room : sub_rooms) {
		if (room->name == room_name) {
			remove_member(member);
			room->add_member(member);

			return room;
		}
	}

	std::shared_ptr<SubRoom> new_room = std::make_shared<SubRoom>(this, room_name);
	remove_member(member);
	new_room->add_member(member);
	sub_rooms.push_back(new_room);
	return new_room;
}

SubRoom::SubRoom(ChatRoom* parent_room, const std::string& name) : ChatRoom(name), parent(parent_room) {};

SubRoom::SubRoom(const SubRoom& other) : ChatRoom(other), parent(other.parent) {};

void SubRoom::move_up(SOCKET member) {
	parent->add_member(member);
	remove_member(member);
}