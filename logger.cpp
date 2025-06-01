#include "logger.hpp"
#include <iostream>
#include <sstream>
#include "json.hpp"
#include <chrono>
#include <iomanip>

Logger::Logger() { // keeps the json file open
	json_stream.open("log.json", std::ios::app);
    active_modules = { LogModule::AUTH,LogModule::MESSAGE,LogModule::CLIENT,LogModule::SERVER,LogModule::NETWORK };
    if (!json_stream.is_open()) {
        std::cerr << "Failed to open log.json\n";
        std::terminate();
    }

}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    time_t t = std::chrono::system_clock::to_time_t(now);

    std::tm loc_time;
    localtime_s(&loc_time, &t);

    std::ostringstream formatted_time;
    formatted_time << std::put_time(&loc_time, "%Y-%m-%d %H:%M:%S");
    return formatted_time.str();
}

std::string Logger::get_log_level(LogLevel level) {
    switch (level) {
    case LogLevel::DEBUG: return "DEBUG";
    case LogLevel::INFO: return "INFO";
    case LogLevel::WARNING: return "WARNING";
    case LogLevel::ERR: return "ERROR";
    default: return "INVALID";
    }
}

std::string Logger::get_log_module(LogModule mod) {
    switch (mod) {
    case LogModule::SERVER: return "SERVER";
    case LogModule::CLIENT: return "CLIENT";
    case LogModule::NETWORK: return "NETWORK";
    case LogModule::AUTH: return "AUTH";
    case LogModule::MESSAGE: return "MESSAGE";
    default: return "INVALID";
    }
}

Logger &Logger::get() {
    static Logger instance;
    return instance;
}

void Logger::set_min_log_level(LogLevel level) {
    m_log_level = level;
}

void Logger::enable_module(LogModule mod) {
    active_modules.insert(mod);
}

void Logger::disable_module(LogModule mod) {
    active_modules.erase(mod);
}

// sends a formatted message into the json file and stdout

void Logger::logi(LogLevel lvl, LogModule mod, const std::string& str) {
    if ((lvl < m_log_level) || (active_modules.find(mod) == active_modules.end())) return;

    nlohmann::json log;
    std::lock_guard<std::mutex> lock(log_mutex);

    log["timestamp"] = get_timestamp();
    log["level"] = get_log_level(lvl);
    log["module"] = get_log_module(mod);
    log["message"] = str;

    json_stream << log.dump(0) << '\n';
    json_stream.flush();

    std::cout << '[' <<
        log["timestamp"].get<std::string>() << "] [" <<
        get_log_level(lvl) << "] [" <<
        get_log_module(mod) << "] [" <<
        str << "]\n";


}