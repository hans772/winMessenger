#include "auth.hpp"
#include "random"
#include "openssl/bio.h"
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <openssl/hmac.h>
#include <iostream>

ServerAuth::ServerAuth() {
	users_f = std::fstream("users.txt");
	if (!users_f) {
		Logger::get().log(LogLevel::ERR, LogModule::AUTH, "Could Not Load Users File");
		return;
	}
	std::string user;
	while (std::getline(users_f, user)) {
		active_users.push_back(user);
	}

	std::fstream fs("serverconfig.ini", (std::ios::in | std::ios::out | std::ios::app));
	if (!fs) {
		Logger::get().log(LogLevel::ERR, LogModule::AUTH, "Could Not Load Server Secret");
		return;

	}
	std::string line;
	while (std::getline(fs, line)) {
		if (line.find("SERVER_SECRET") != std::string::npos) {
			secret = std::string(std::find(line.begin(), line.end(), '=')+1, line.end());
			break;
		}
	}
	if (!secret.length()) {
		const char usable[] = "0123456789" "qwertyuiopasdfghjklzxcvbnm" "QWERTYUIOPASDFGHJKLZXCVBNM";
		std::random_device rd;
		std::mt19937 generator(rd());
		std::uniform_int_distribution<> distr(0, sizeof(usable) - 2); //-2 cuz ignoring \n

		for (int i = 0; i < 32; i++) secret += usable[distr(generator)];

		Logger::get().log(LogLevel::INFO, LogModule::AUTH, "Created New Server Secret");
		std::string wline = "SERVER_SECRET=" + secret + "\n";
		fs.write(wline.c_str(), wline.length());
		fs.flush();
	}

	fs.close();
}
std::string ServerAuth::get_token(const std::string &username) const {
	std::string signature = hmac_sha256(secret, username);
	return username + "." + base64_enc(signature);
}

int ServerAuth::verify_token(const std::string& token) const {

	int delim_pos = token.find('.');
	if(delim_pos == std::string::npos) return 0;
	
	std::string usn = token.substr(0, delim_pos);
	std::string sign = base64_dec(token.substr(delim_pos + 1, token.length()));

	if (sign == hmac_sha256(secret, usn)) return 1;
	else return 0;
}

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

void ServerAuth::add_user(std::string username) {
	active_users.push_back(username);

}


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

std::string ClientAuth::get_username(const std::string &ip) {
	std::string token = tokens[ip].get<std::string>();
	return token.substr(0, token.find('.'));
}

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