#include <iostream>
#include <unordered_map>
#include <stdint.h>
#include <string>
#include <regex>
#include <cstring>

#include "message.hpp"

using namespace std;
using nlohmann::json;

/*
	-----------------
	Message Format
	-----------------

	Header Length : Int_32 : Holds the length of the header.
	Header : Serialized JSON string : Holds all necessary information of the message and body
		body_length
		body_type
		message_type
		sender
	Body : Contains body if required.

*/


// Default constructor for message class, initializes all values to default type

Message::Message(MessageType type) 
{
	headerjson["body_length"] = 0;
	headerjson["body_type"] = 0;
	headerjson["message_type"] = type;
	headerjson["sender"] = "DEFAULT";
	body_i = 0;
	body_s = "";
}

// Default getters and setters for the message class

json Message::get_header() { return headerjson; }
string Message::get_sender() { return headerjson["sender"].get<string>(); }
int Message::get_bodylen() { return headerjson["body_length"].get<int>(); }
int Message::get_type() { return headerjson["message_type"].get<int>(); }

int Message::get_body_type() { return headerjson["body_type"];  }
string Message::get_body_str() { return body_s; }
int Message::get_body_int() { return ntohl(body_i); }
nlohmann::json Message::get_body_json() { return body_json; }

void Message::set_type(MessageType type) { headerjson["message_type"] = type; }
void Message::set_body_type(int body_type) { headerjson["body_type"] = body_type; }
void Message::set_sender(string sender) { headerjson["sender"] = sender; }

void Message::set_body_string(string str) {
	body_s = str;

	headerjson["body_length"] = str.length();
	headerjson["body_type"] = MSGBODY_STRING;
}
void Message::set_body_int(int integ){
	body_i = int32_t(integ);

	headerjson["body_length"] = sizeof(int32_t);
	headerjson["body_type"] = MSGBODY_INT;
}

void Message::set_body_json(json json) {
	body_json = json;

	headerjson["body_length"] = body_json.dump().length();
	headerjson["body_type"] = MSGBODY_JSON;
}

// Using JSON serialization for serializing and deserializing headers

pair<string, int> Message::serialize_header() {
	string serialized_header = headerjson.dump();
	return pair<string, int>(serialized_header, serialized_header.length());
}

json Message::deserialize_header(string msg) {
	json header = json::parse(msg);
	return header;
}

Message Message::recieve_message(SOCKET socket) {

	/*
		Recieves message from socket passed in `socket` param.
		This function will take a socket as a parameter and will recieve a complete message from the socket
		It will compile the data recieved into a message class and return it.

		This function will block flow of execution until message is recieved.
	*/

	if (socket == INVALID_SOCKET) return Message(ERRORMSG);

	// Recieving headerlen from socket, headerlen is of type Int_32 which is fixed to 4 bytes.

	int32_t headerlen = 0;
	char* headlen = (char*)&headerlen;
	int rem = sizeof(headerlen);

	int rc;

	// Each recv call need not necessarily get all the data, hence we loop until remaining bytes to be recieved
	// denoted by `rem` reaches 0, i.e. all bytes have been recieved.

	do {
		rc = recv(socket, headlen, rem, 0);
		if (rc <= 0) {
			Message ret = Message(ERRORMSG);
			ret.set_body_int(WSAGetLastError());

			return ret;
		}
		else {

			// moving headlen pointer forward by recieved bytes and decrementing the remaining bytes to be recieved

			headlen += rc;
			rem -= rc;
		}
	} while (rem > 0);

	// Creating a buffer to recieve the header into, the headerlen gives the length of the header
	// so we know the length of the buffer to make, this helps in managing memory, as we can create a buffer of 
	// exact header length instead of a default buffer length, reducing wastage.

	char* headerbuf = new char[ntohl(headerlen)];
	rem = ntohl(headerlen);

	char* p = headerbuf;

	do {
		rc = recv(socket, p, rem, 0);
		if (rc <= 0) {

			// Error handling, also make sure to delete dynamically allocated memory to avoid memory leaks.

			Message ret = Message(ERRORMSG);
			ret.set_body_int(WSAGetLastError());
			delete[] headerbuf;
			return ret;
		}
		else {				
			p += rc;
			rem -= rc;
			}
	} while (rem > 0);

	// The recieved byte array is now deserialized to convert to json format.

	json header = Message::deserialize_header(string(headerbuf, ntohl(headerlen)));

	Message return_msg = Message((MessageType)header["message_type"].get<int>());
	return_msg.set_sender(header["sender"].get<string>());
	return_msg.set_body_type(header["body_type"].get<int>());
	// Body length is specified inside the header, which can be used to create another buffer.

	rem = header["body_length"].get<int>();
	char* bodybuf = new char[rem];

	p = bodybuf;
	
	do {
		rc = recv(socket, p, rem, 0);
		if (rc <= 0) {
			Message ret = Message(ERRORMSG);
			ret.set_body_int(WSAGetLastError());
			delete[] bodybuf;
			delete[] headerbuf;
			return ret;
		}
		else {
			p += rc;
			rem -= rc;
		}
	} while (rem > 0);

	// Creating the return message which will be returned.
	// Body type will be specified in the message header.

	return_msg.set_body_from_buffer(return_msg.get_body_type(), bodybuf, header["body_length"].get<int>());
	return_msg.set_sender(header["sender"].get<string>());
	
	delete[] bodybuf;
	delete[] headerbuf;

	return return_msg;
}

void Message::set_body_from_buffer(int type, char* bodybuf, int body_length) {
	switch (type) {
	case MSGBODY_STRING:
	{
		string body_s;
		body_s.assign(bodybuf, bodybuf + body_length);
		set_body_string(body_s);
		break;
	}
	case MSGBODY_INT:
	{
		int32_t body_i;
		memcpy(&body_i, bodybuf, sizeof(int32_t));
		set_body_int(body_i);
		break;
	}
	case MSGBODY_JSON:
	{
		string body_s;
		body_s.assign(bodybuf, bodybuf + body_length);
		json body_js = json::parse(body_s);
		set_body_json(body_js);
		break;
	}
	}
}

int Message::send_message(SOCKET socket) {

	/*
		This class function will convert the message into byte array and send it in the order of bytes specified
		in the message format. It takes a socket as input and sends the data across that socket.
	*/
	
	// Header is serialized.

	pair<string, int> headerinfo = serialize_header();

	// The memory location of the header len is accessed, and iterated through to access the byte sequence.

	int32_t headerlen = htonl(headerinfo.second);
	char* headlenbuf = (char*)&headerlen;
	int rem = sizeof(headerlen);
	int sd;

	// The sending follows a similar process to recieving, since bytes sent may not be the same as the intended amount
	// Sent bytes will be stored in `sd` and will decrement it from the remaining bytes to be sent.

	do {
		sd = send(socket, headlenbuf, rem, 0);
		if (sd <= 0) {
			cout << "Error sending headerlen to socket: " << WSAGetLastError() << endl;
			return -1;
		}
		else {
			headlenbuf += sd;
			rem -= sd;
		}
	} while (rem > 0);

	// The serialized header is copied into a buffer.

	char* serheadbuf = new char[headerinfo.first.length()];
	strncpy(serheadbuf, headerinfo.first.c_str(), headerinfo.first.length());
	rem = headerinfo.second;

	char* p = serheadbuf;

	do {
		sd = send(socket, p, rem, 0);
		if (sd <= 0) {
			cout << "Error sending header to socket: " << WSAGetLastError() << endl;

			// Memory management.

			delete[] serheadbuf;
			return -1;
		}
		else {
			p += sd;
			rem -= sd;
		}
	} while (rem > 0);

	delete[] serheadbuf;

	// Body is also sent the same way as header.

	rem = headerjson["body_length"].get<int>();

	if (!rem) { return 1; }

	char* bodybuf;

	// The body is converted to its respective type before being sent through the socket.

	switch (headerjson["body_type"].get<int>()) {
	case MSGBODY_STRING:
		bodybuf = new char[headerjson["body_length"].get<int>() + 1];
		memcpy(bodybuf, body_s.c_str(), headerjson["body_length"].get<int>() + 1);
		break;
	case MSGBODY_INT:
		bodybuf = new char[sizeof(int32_t)];
		memcpy(bodybuf, &body_i, sizeof(int32_t));
		break;
	case MSGBODY_JSON:
		bodybuf = new char[headerjson["body_length"].get<int>() + 1];
		memcpy(bodybuf, body_json.dump().c_str(), headerjson["body_length"].get<int>() + 1);
		break;
	default:
		cout << "Unknown body type" << endl;
		return -1;
	}

	p = bodybuf;

	do {
		sd = send(socket, p, rem, 0);
		if (sd <= 0) {
			cout << "Error sending body to socket: " << WSAGetLastError() << endl;
			delete[] bodybuf;
			return -1;
		}
		else {
			p += sd;
			rem -= sd;
		}
	} while (rem > 0);

	delete[] bodybuf;
	return 1;
}

void Message::print_message() {

	// Prints out the message (debugging and server logs)

	cout << "---------------------\n";
	cout << "   MESSAGE RECIEVED  \n";
	cout << "---------------------\n";
	cout << "Message Type: " << get_type() << '\n';
	cout << "Message Sender: " << get_sender() << '\n';
	cout << "Body Type: " << headerjson["body_type"].get<int>() << '\n';
	cout << "Body Length: " << headerjson["body_length"].get<int>() << '\n';
	switch(headerjson["body_type"].get<int>()) {
	case MSGBODY_STRING:
		cout << "Body: " << body_s << '\n';
		break;
	case MSGBODY_INT:
		cout << "Body: " << body_i << '\n';
		break;
	}
	cout << "_______________________" << endl;
}