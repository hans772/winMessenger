#pragma once

#include <mutex>
#include <fstream>
#include <set>
#include <string>
#include <sstream>
enum class LogModule {
	CLIENT, 
	SERVER, 
	NETWORK, 
	MESSAGE,
	AUTH
};

enum class LogLevel {
	DEBUG = 0, 
	INFO = 1, 
	WARNING = 2, 
	ERR = 3
};

class Logger {
	std::mutex log_mutex;
	std::ofstream json_stream;

	LogLevel m_log_level;
	std::set<LogModule> active_modules;

	Logger();

	std::string get_timestamp();
	std::string get_log_level(LogLevel level);
	std::string get_log_module(LogModule mod);

public:

	static Logger& get();
	void set_min_log_level(LogLevel level);
	void enable_module(LogModule mod);
	void disable_module(LogModule mod);

	void logi(LogLevel lvl, LogModule mod, const std::string &str);

	template<typename... LogArgs>
	void log(LogLevel lvl, LogModule mod, LogArgs&&... args) {
		std::ostringstream merge;
		(merge << ... << args);  // fold expression (C++17)
		logi(lvl, mod, merge.str());
	}

	Logger(const Logger&) = delete; //no copying
	void operator=(const Logger&) = delete;
};