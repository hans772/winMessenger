#pragma once

#include <fstream>
#include <sstream>
#include "logger.hpp"
#include "json.hpp"

class ServerAuth {

	// each server has a secret. stored in the authorizer

	std::string secret;
	std::vector<std::string> active_users;

	// encoding and signing algorithms, const cuz they dont change anything

	std::string base64_enc(const std::string& value) const ;
	std::string hmac_sha256(const std::string& key, const std::string& data) const ;
	std::string base64_dec(const std::string& input) const;

	// mutex so that active users is only accessed once at a time.

	std::mutex users_mutex; 

public:
	ServerAuth();

	//getting and verifying tokens, server dependent.

	std::string get_token(const std::string &username) const;
	int verify_token(const std::string& token) const;
	
	// adding users and checking for users

	void add_user(std::string username);
	int check_user(std::string username);
};

class ClientAuth {

	//json, because a single device can have multiple tokens for different servers it connects to.
	nlohmann::json tokens;

public:

	ClientAuth();

	// getting username from token
	std::string get_username(const std::string &ip);

	// token setter and getter and checker (ip dependent)

	void set_token(const std::string &tok, const std::string &ip);
	std::string get_token(const std::string& ip);
	int check_token(const std::string& ip);
};