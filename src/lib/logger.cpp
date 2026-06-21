#include "logger.h"

#include <fstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <regex>
#include <sstream>

namespace mining {

namespace {

std::string redact_sensitive_fields(const std::string& input) {
    if (input.empty()) {
        return input;
    }

    std::string out = input;
    const std::regex quoted_kv(
        "(\\\"(?:password|passwd|secret|token|auth_token|username|payout_address)\\\"\\s*:\\s*\\\")([^\\\"]*)(\\\")",
        std::regex_constants::icase);
    out = std::regex_replace(out, quoted_kv, "$1***$3");

    const std::regex plain_kv(
        "((?:password|passwd|secret|token|auth_token|username|payout_address)\\s*[=:]\\s*)([^,;\\s]+)",
        std::regex_constants::icase);
    out = std::regex_replace(out, plain_kv, "$1***");

    return out;
}

std::string escape_json_string(const std::string& input) {
    std::string out;
    out.reserve(input.size() + 8);
    for (char ch : input) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

} // namespace

Logger::Logger(const std::string& filename, LogLevel level)
    : filename_(filename), level_(level) {}

Logger::~Logger() = default;

std::string Logger::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(6) << (ms.count() * 1000) << 'Z';
    return oss.str();
}

void Logger::log(LogLevel level, const std::string& component, const std::string& node_id,
                 const std::string& message, const std::string& context) {
    if (level < level_) {
        return;
    }

    std::string level_str;
    switch (level) {
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO: level_str = "INFO"; break;
        case LogLevel::WARN: level_str = "WARN"; break;
        case LogLevel::ERR: level_str = "ERROR"; break;
    }

    const std::string safe_message = redact_sensitive_fields(message);
    const std::string safe_context = redact_sensitive_fields(context);
    const std::string esc_component = escape_json_string(component);
    const std::string esc_node = escape_json_string(node_id);
    const std::string esc_message = escape_json_string(safe_message);
    const std::string esc_context = escape_json_string(safe_context);

    std::ostringstream json_line;
    json_line << "{"
              << "\"timestamp\":\"" << get_timestamp() << "\","
              << "\"level\":\"" << level_str << "\","
              << "\"component\":\"" << esc_component << "\","
              << "\"node_id\":\"" << esc_node << "\","
              << "\"message\":\"" << esc_message << "\"";
    if (!esc_context.empty()) {
        json_line << ",\"context\":\"" << esc_context << "\"";
    }
    json_line << "}";

    std::ofstream log_file(filename_, std::ios_base::app);
    if (log_file.is_open()) {
        log_file << json_line.str() << "\n";
    }

    // Also print to console for debugging
    std::cout << json_line.str() << "\n";
}

void Logger::debug(const std::string& component, const std::string& node_id,
                   const std::string& message, const std::string& context) {
    log(LogLevel::DEBUG, component, node_id, message, context);
}

void Logger::info(const std::string& component, const std::string& node_id,
                  const std::string& message, const std::string& context) {
    log(LogLevel::INFO, component, node_id, message, context);
}

void Logger::warn(const std::string& component, const std::string& node_id,
                  const std::string& message, const std::string& context) {
    log(LogLevel::WARN, component, node_id, message, context);
}

void Logger::error(const std::string& component, const std::string& node_id,
                   const std::string& message, const std::string& context) {
    log(LogLevel::ERR, component, node_id, message, context);
}

} // namespace mining
