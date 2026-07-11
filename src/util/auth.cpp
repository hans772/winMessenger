#include "auth.hpp"
#include "random"
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/hmac.h>
#include <iostream>

ServerAuth::ServerAuth() {
	/*
	On initializing a server, it creates an Auth which loads previous users 
	it also loads the server secret, if not found, a new secret is generated.
	*/

	std::ifstream users_f = std::ifstream("users.txt");
	if (!users_f) {
		Logger::get().log(LogLevel::ERR, LogModule::AUTH, "Could Not Load Users File");
		return;
	}
	std::string user;
	while (std::getline(users_f, user)) {
		active_users.push_back(user);
	}

	std::fstream fs("serverconfig.ini", std::ios::in); // file stream to load server secret
	if (!fs) {
		Logger::get().log(LogLevel::ERR, LogModule::AUTH, "Could Not Load Server Secret");
		return;

	}
	std::string line;
	while (std::getline(fs, line)) { // checks every line in the serverconfig folder for SERVER_SECRET=...
		if (line.find("SERVER_SECRET") != std::string::npos) {
			secret = std::string(std::find(line.begin(), line.end(), '=')+1, line.end());
			break;
		}
	}
	// this allows for future compatibility, other config options can be added

	fs.close();

	// generates a secret of length 32

	if (!secret.length()) {
		const char usable[] = "0123456789" "qwertyuiopasdfghjklzxcvbnm" "QWERTYUIOPASDFGHJKLZXCVBNM";
		std::random_device rd;
		std::mt19937 generator(rd());
		std::uniform_int_distribution<> distr(0, sizeof(usable) - 2); //-2 cuz ignoring \0

		std::ofstream ofs("serverconfig.ini", std::ios::app);

		for (int i = 0; i < 32; i++) secret += usable[distr(generator)];

		Logger::get().log(LogLevel::INFO, LogModule::AUTH, "Created New Server Secret: ", secret);
		std::string wline = "SERVER_SECRET=" + secret + "\n";
		ofs.write(wline.c_str(), wline.length());
		ofs.close();
	}
}

// returns a signed token, its the signed form of the username with the server key
// this allows for verification of the token, to check if it has been tampered with or not.

std::string ServerAuth::get_token(const std::string &username) const {
	std::string signature = hmac_sha256(secret, username); 
	return username + "." + base64_enc(signature);
}

// verifies the recieved token, it reverses the operations done on it and checks if the username decoded returns the same value as the sign

int ServerAuth::verify_token(const std::string& token) const {

	int delim_pos = token.find('.');
	if(delim_pos == std::string::npos) return 0;
	
	std::string usn = token.substr(0, delim_pos);
	std::string sign = base64_dec(token.substr(delim_pos + 1, token.length()));

	if (sign == hmac_sha256(secret, usn)) return 1;
	else return 0;
}

// encoding and decoding in base64, hmac sha256 returns values in binary which cannot be sent over sockets or stored in json
// hence we encode it to base64

std::string ServerAuth::base64_enc(const std::string &value) const {
	BIO* encoder = BIO_new(BIO_f_base64());
	BIO_set_flags(encoder, BIO_FLAGS_BASE64_NO_NL);

	BIO* encoded = BIO_new(BIO_s_mem());
	encoded = BIO_push(encoder, encoded); // now every push willl be sent through the encoder first

	BIO_write(encoded, reinterpret_cast<const unsigned char*>(value.c_str()), value.length());
	BIO_flush(encoded);

	BUF_MEM* bptr;
	BIO_get_mem_ptr(encoded, &bptr);

	std::string result(bptr->data, bptr->length);

	BIO_free_all(encoded);

	return result;

}

std::string ServerAuth::base64_dec(const std::string& input) const {
	BIO* bio, * b64;
	char* buffer = (char*)malloc(input.length());
	memset(buffer, 0, input.length());

	b64 = BIO_new(BIO_f_base64());
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // important
	bio = BIO_new_mem_buf(input.data(), input.length());
	bio = BIO_push(b64, bio);

	int decoded_len = BIO_read(bio, buffer, input.length());
	BIO_free_all(bio);

	if (decoded_len <= 0) return "INVALID";

	return std::string(buffer, decoded_len);
}

// hmac sha256 signing

std::string ServerAuth::hmac_sha256(const std::string& key, const std::string& data) const {
	unsigned int len = EVP_MAX_MD_SIZE;
	unsigned char hash[EVP_MAX_MD_SIZE];

	HMAC_CTX* ctx = HMAC_CTX_new();
	HMAC_Init_ex(ctx, key.c_str(), key.length(), EVP_sha256(), NULL);
	HMAC_Update(ctx, reinterpret_cast<const unsigned char*>(data.c_str()), data.length());
	HMAC_Final(ctx, hash, &len);
	HMAC_CTX_free(ctx);

	return std::string(reinterpret_cast<char*>(hash), len);  // raw binary
}

// adds a user to the file and the vector

void ServerAuth::add_user(std::string username) {
	std::lock_guard<std::mutex> lock(users_mutex);

	active_users.push_back(username);

	std::ofstream users_f = std::ofstream("users.txt", std::ios::app);
	if (!users_f) {
		Logger::get().log(LogLevel::ERR, LogModule::AUTH, "Could Not Load Users File");
		return;
	}
	users_f.write((username + '\n').c_str(), username.length() + 1);
}

//checks if a user is already registered

int ServerAuth::check_user(std::string username) {
	std::lock_guard<std::mutex> lock(users_mutex);

	return std::find(active_users.begin(), active_users.end(), username) != active_users.end();
}


// client auth does not need a mutex as it is handled by a single thread

ClientAuth::ClientAuth() {
	std::fstream fs("clientconfig.json", std::ios::in);
	if (!fs) {
		Logger::get().log(LogLevel::ERR, LogModule::AUTH, "Could Not Load clientconfig.json");
		return;

	}
	
	fs >> tokens;
	Logger::get().log(LogLevel::INFO, LogModule::AUTH, "Loaded Client Tokens");
	fs.close();
}

// get username registered to an IP
std::string ClientAuth::get_username(const std::string &ip) {
	std::string token = tokens[ip].get<std::string>();
	return token.substr(0, token.find('.'));
}

// get token, check token and set token registered to an IP
std::string ClientAuth::get_token(const std::string& ip) {
	return tokens[ip].get<std::string>();
}

int ClientAuth::check_token(const std::string& ip) {
	return tokens.contains(ip);
}

void ClientAuth::set_token(const std::string &tok, const std::string &ip) {
	tokens[ip] = tok;

	std::fstream fs("clientconfig.json", std::ios::out);
	if (!fs) {
		Logger::get().log(LogLevel::ERR, LogModule::AUTH, "Could Not Load clientconfig.ini");
		return;
	}
	
	fs << tokens;
	fs.flush();
	fs.close();
}