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

enum class LogLevel { // in order
	DEBUG = 0, 
	INFO = 1, 
	WARNING = 2, 
	ERR = 3
};

class Logger {
	std::mutex log_mutex; // one log happens at once, so that no writing overlaps occur
	std::ofstream json_stream;

	LogLevel m_log_level; // the minimum log level allowed
	std::set<LogModule> active_modules; // active modules

	Logger();

	std::string get_timestamp(); //gets current timestamp of the log
	std::string get_log_level(LogLevel level); // gets string form of log level
	std::string get_log_module(LogModule mod); // gets string form of log module

public:

	static Logger& get(); //one logger per program instance
	void set_min_log_level(LogLevel level); // sets the minimum permissible log level, logs below this level are ignored
	void enable_module(LogModule mod); // enabler and disabler for log module
	void disable_module(LogModule mod);

	void logi(LogLevel lvl, LogModule mod, const std::string &str);

	template<typename... LogArgs>
	void log(LogLevel lvl, LogModule mod, LogArgs&&... args) { // allows for multiple arguments, makes it simpler than making complex lines in code
		std::ostringstream merge;
		(merge << ... << args);  // fold expression (C++17)
		logi(lvl, mod, merge.str());
	}

	Logger(const Logger&) = delete; //no copying
	void operator=(const Logger&) = delete;
};