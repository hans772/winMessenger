#ifndef CLIENT_HPP
#define CLIENT_HPP

#include "net/wmnet.hpp"

#include <string>
#include <mutex>
#include <atomic>

class ChatClient : public WMNet::Client {
	
	std::thread input_thread;
	std::mutex console_mtx;

	std::string current_input;

	std::atomic<bool> awaiting_login;

	std::string name;

	void clear_input_line();
	void restore_input();

	void handle_input(const std::string& input);

	void on_connect_to_server() override;
	void on_disconnect() override;
	void on_client_message(WMNet::ServerMessage msg);
	void on_request_login(WMNet::ServerMessage msg);
	void on_acknowledge_login(WMNet::ServerMessage msg);
	void on_server_message(WMNet::ServerMessage msg);

public:
	void input_loop();
	ChatClient();
};

#endif // !CLIENT_HPP
