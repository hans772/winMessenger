#ifndef ROOMS_HPP
#define ROOMS_HPP

#include <iostream>
#include <mutex>
#include <vector>
#include <WinSock2.h>
#include <set>

class SubRoom;

class ChatRoom
{
public:
	std::string name;
	std::shared_ptr<std::mutex> room_mutex;
	std::set<SOCKET> members;

	ChatRoom(std::string room_name);
	ChatRoom(const ChatRoom& other);

	void add_member(SOCKET member);
	void remove_member(SOCKET member);
	std::vector< std::shared_ptr<SubRoom>> sub_rooms;

	std::shared_ptr<SubRoom> move_member_to_subroom(std::string room_name, SOCKET member);
};

class SubRoom : public ChatRoom
{
private:
	ChatRoom* parent;

public:

	SubRoom(ChatRoom* parent_room, const std::string& name);
	SubRoom(const SubRoom& other);

	void move_up(SOCKET member);
};

#endif // !ROOMS_HPP
