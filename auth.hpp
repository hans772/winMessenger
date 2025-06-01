#pragma once

#include <fstream>
#include <sstream>
#include "logger.hpp"
#include "json.hpp"

class ServerAuth {
	std::fstream users_f;
	std::string secret;
	std::string base64_enc(const std::string& value) const ;
	std::string hmac_sha256(const std::string& key, const std::string& data) const ;
	std::string base64_dec(const std::string& input) const;

public:
	std::vector<std::string> active_users;
	ServerAuth();

	std::string get_token(const std::string &username) const;
	int verify_token(const std::string& token) const;
	
	void add_user(std::string username);
};

class ClientAuth {
	nlohmann::json tokens;

public:
	ClientAuth();

	std::string get_username(const std::string &ip) ;
	void set_token(const std::string &tok, const std::string &ip);
	std::string get_token(const std::string& ip);
	int check_token(const std::string& ip);
};