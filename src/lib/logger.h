#pragma once

#include <string>
#include <vector>
#include <ctime>

namespace mining {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARN = 2,
    ERR = 3
};

// Simple JSON-line logger
class Logger {
public:
    Logger(const std::string& filename, LogLevel level = LogLevel::INFO);
    ~Logger();

    void log(LogLevel level, const std::string& component, const std::string& node_id, 
             const std::string& message, const std::string& context = "");

    void debug(const std::string& component, const std::string& node_id, const std::string& message, 
               const std::string& context = "");
    void info(const std::string& component, const std::string& node_id, const std::string& message, 
              const std::string& context = "");
    void warn(const std::string& component, const std::string& node_id, const std::string& message, 
              const std::string& context = "");
    void error(const std::string& component, const std::string& node_id, const std::string& message, 
               const std::string& context = "");

private:
    std::string filename_;
    LogLevel level_;
    std::string get_timestamp() const;
};

} // namespace mining
