#include "miner_config.h"

#include "logger.h"

#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_map>

namespace {

std::string read_text_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return {};
    }

    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

struct JsonValue {
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Object
    };

    Type type = Type::Null;
    bool bool_value = false;
    std::uint64_t number_value = 0;
    std::string string_value;
    std::unordered_map<std::string, JsonValue> object_value;
};

class JsonParser {
public:
    explicit JsonParser(const std::string& input) : input_(input) {}

    bool parse(JsonValue& out) {
        skip_ws();
        if (!parse_value(out)) {
            return false;
        }
        skip_ws();
        return pos_ == input_.size();
    }

private:
    const std::string& input_;
    std::size_t pos_ = 0;

    bool parse_value(JsonValue& out) {
        skip_ws();
        if (pos_ >= input_.size()) {
            return false;
        }

        const char ch = input_[pos_];
        if (ch == '{') {
            return parse_object(out);
        }
        if (ch == '"') {
            out.type = JsonValue::Type::String;
            return parse_string(out.string_value);
        }
        if (ch == 't' || ch == 'f') {
            out.type = JsonValue::Type::Bool;
            return parse_bool(out.bool_value);
        }
        if (ch == 'n') {
            return parse_null(out);
        }
        if (ch >= '0' && ch <= '9') {
            out.type = JsonValue::Type::Number;
            return parse_number(out.number_value);
        }

        return false;
    }

    bool parse_object(JsonValue& out) {
        if (pos_ >= input_.size() || input_[pos_] != '{') {
            return false;
        }
        ++pos_;

        out.type = JsonValue::Type::Object;
        out.object_value.clear();

        skip_ws();
        if (pos_ < input_.size() && input_[pos_] == '}') {
            ++pos_;
            return true;
        }

        while (pos_ < input_.size()) {
            skip_ws();
            std::string key;
            if (!parse_string(key)) {
                return false;
            }

            skip_ws();
            if (pos_ >= input_.size() || input_[pos_] != ':') {
                return false;
            }
            ++pos_;

            JsonValue value;
            if (!parse_value(value)) {
                return false;
            }
            out.object_value[key] = value;

            skip_ws();
            if (pos_ >= input_.size()) {
                return false;
            }
            if (input_[pos_] == '}') {
                ++pos_;
                return true;
            }
            if (input_[pos_] != ',') {
                return false;
            }
            ++pos_;
        }

        return false;
    }

    bool parse_string(std::string& out) {
        if (pos_ >= input_.size() || input_[pos_] != '"') {
            return false;
        }
        ++pos_;

        std::ostringstream oss;
        while (pos_ < input_.size()) {
            const char ch = input_[pos_++];
            if (ch == '"') {
                out = oss.str();
                return true;
            }
            if (ch == '\\') {
                if (pos_ >= input_.size()) {
                    return false;
                }
                const char esc = input_[pos_++];
                switch (esc) {
                    case '"': oss << '"'; break;
                    case '\\': oss << '\\'; break;
                    case '/': oss << '/'; break;
                    case 'b': oss << '\b'; break;
                    case 'f': oss << '\f'; break;
                    case 'n': oss << '\n'; break;
                    case 'r': oss << '\r'; break;
                    case 't': oss << '\t'; break;
                    default: return false;
                }
                continue;
            }
            oss << ch;
        }

        return false;
    }

    bool parse_number(std::uint64_t& out) {
        if (pos_ >= input_.size() || input_[pos_] < '0' || input_[pos_] > '9') {
            return false;
        }
        std::size_t start = pos_;
        while (pos_ < input_.size() && input_[pos_] >= '0' && input_[pos_] <= '9') {
            ++pos_;
        }
        try {
            out = std::stoull(input_.substr(start, pos_ - start));
            return true;
        } catch (...) {
            return false;
        }
    }

    bool parse_bool(bool& out) {
        if (input_.compare(pos_, 4, "true") == 0) {
            out = true;
            pos_ += 4;
            return true;
        }
        if (input_.compare(pos_, 5, "false") == 0) {
            out = false;
            pos_ += 5;
            return true;
        }
        return false;
    }

    bool parse_null(JsonValue& out) {
        if (input_.compare(pos_, 4, "null") != 0) {
            return false;
        }
        pos_ += 4;
        out.type = JsonValue::Type::Null;
        return true;
    }

    void skip_ws() {
        while (pos_ < input_.size()) {
            const char ch = input_[pos_];
            if (ch == ' ' || ch == '\n' || ch == '\r' || ch == '\t') {
                ++pos_;
                continue;
            }
            break;
        }
    }
};

const JsonValue* object_field(const JsonValue* obj, const std::string& key) {
    if (obj == nullptr || obj->type != JsonValue::Type::Object) {
        return nullptr;
    }
    auto it = obj->object_value.find(key);
    if (it == obj->object_value.end()) {
        return nullptr;
    }
    return &it->second;
}

bool read_string_field(const JsonValue* obj, const std::string& key, std::string& out) {
    const JsonValue* field = object_field(obj, key);
    if (field == nullptr || field->type != JsonValue::Type::String) {
        return false;
    }
    out = field->string_value;
    return true;
}

bool read_uint_field(const JsonValue* obj, const std::string& key, std::uint64_t& out) {
    const JsonValue* field = object_field(obj, key);
    if (field == nullptr || field->type != JsonValue::Type::Number) {
        return false;
    }
    out = field->number_value;
    return true;
}

bool read_bool_field(const JsonValue* obj, const std::string& key, bool& out) {
    const JsonValue* field = object_field(obj, key);
    if (field == nullptr || field->type != JsonValue::Type::Bool) {
        return false;
    }
    out = field->bool_value;
    return true;
}

} // namespace

bool load_config(const std::string& path, MinerConfig& cfg) {
    cfg = MinerConfig{};
    const std::string json = read_text_file(path);
    if (json.empty()) {
        return false;
    }

    JsonValue root;
    JsonParser parser(json);
    if (!parser.parse(root) || root.type != JsonValue::Type::Object) {
        return false;
    }

    const JsonValue* pool = object_field(&root, "pool");
    const JsonValue* hashing = object_field(&root, "hashing");
    const JsonValue* logging = object_field(&root, "logging");

    std::string text_value;
    std::uint64_t uint_value = 0;

    if (read_string_field(&root, "node_id", text_value)) {
        cfg.node_id = text_value;
    }
    if (read_string_field(&root, "worker_id", text_value)) {
        cfg.worker_id = text_value;
    }
    if (read_string_field(&root, "payout_address", text_value)) {
        cfg.payout_address = text_value;
    }
    if (read_string_field(pool, "username", text_value) || read_string_field(&root, "username", text_value)) {
        cfg.pool_username = text_value;
    }
    if (read_string_field(pool, "host", text_value) || read_string_field(&root, "host", text_value)) {
        cfg.pool_host = text_value;
    }
    if ((read_uint_field(pool, "port", uint_value) || read_uint_field(&root, "port", uint_value))
        && uint_value > 0U && uint_value <= 65535U) {
        cfg.pool_port = static_cast<std::uint32_t>(uint_value);
    }
    if (read_string_field(pool, "password", text_value) || read_string_field(&root, "password", text_value)) {
        cfg.pool_password = text_value;
    }
    if (read_string_field(pool, "password_env", text_value) || read_string_field(&root, "password_env", text_value)) {
        cfg.pool_password_env = text_value;
    }
    if (read_bool_field(pool, "enabled", cfg.pool_enabled) || read_bool_field(&root, "enabled", cfg.pool_enabled)) {
    }
    if ((read_uint_field(pool, "notify_timeout_sec", uint_value) || read_uint_field(&root, "notify_timeout_sec", uint_value))
        && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.pool_notify_timeout_sec = static_cast<std::uint32_t>(uint_value);
    }
    if ((read_uint_field(pool, "max_cycles", uint_value) || read_uint_field(&root, "max_cycles", uint_value))
        && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.pool_max_cycles = static_cast<std::uint32_t>(uint_value);
    }
    if ((read_uint_field(pool, "max_reconnect_attempts", uint_value) || read_uint_field(&root, "max_reconnect_attempts", uint_value))
        && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.pool_max_reconnect_attempts = static_cast<std::uint32_t>(uint_value);
    }
    if ((read_uint_field(pool, "reconnect_initial_sec", uint_value) || read_uint_field(&root, "reconnect_initial_sec", uint_value))
        && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.pool_reconnect_initial_sec = static_cast<std::uint32_t>(uint_value);
    }
    if ((read_uint_field(pool, "reconnect_max_sec", uint_value) || read_uint_field(&root, "reconnect_max_sec", uint_value))
        && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.pool_reconnect_max_sec = static_cast<std::uint32_t>(uint_value);
    }
    if (read_string_field(hashing, "prefix", text_value) || read_string_field(&root, "prefix", text_value)) {
        cfg.prefix = text_value;
    }
    if ((read_uint_field(hashing, "difficulty_bits", uint_value) || read_uint_field(&root, "difficulty_bits", uint_value))
        && uint_value <= 255U) {
        cfg.difficulty_bits = static_cast<std::uint32_t>(uint_value);
    }
    if ((read_uint_field(hashing, "threads", uint_value) || read_uint_field(&root, "threads", uint_value))
        && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.thread_count = static_cast<std::uint32_t>(uint_value);
    }
    if ((read_uint_field(hashing, "report_interval_ms", uint_value) || read_uint_field(&root, "report_interval_ms", uint_value))
        && uint_value > 0U && uint_value <= std::numeric_limits<std::uint32_t>::max()) {
        cfg.report_interval_ms = static_cast<std::uint32_t>(uint_value);
    }
    if (read_string_field(logging, "output", text_value) || read_string_field(&root, "output", text_value)) {
        cfg.log_output = text_value;
    }
    if (read_string_field(logging, "health_output", text_value) || read_string_field(&root, "health_output", text_value)) {
        cfg.health_output = text_value;
    }

    return true;
}

std::string pool_username(const MinerConfig& cfg) {
    if (!cfg.pool_username.empty()) {
        return cfg.pool_username;
    }
    if (cfg.payout_address.empty()) {
        return cfg.worker_id;
    }
    return cfg.payout_address + "." + cfg.worker_id;
}

void apply_secret_overrides_from_env(MinerConfig& cfg, mining::Logger& logger) {
    if (cfg.pool_password_env.empty()) {
        return;
    }

#ifdef _WIN32
    char* env_val = nullptr;
    std::size_t env_len = 0;
    const errno_t env_rc = _dupenv_s(&env_val, &env_len, cfg.pool_password_env.c_str());
    const bool missing = (env_rc != 0 || env_val == nullptr || env_len == 0 || env_val[0] == '\0');
    if (missing) {
        if (env_val != nullptr) {
            std::free(env_val);
        }
        logger.warn("config", cfg.node_id, "pool.password_env is set but environment variable is missing", cfg.pool_password_env);
        return;
    }

    cfg.pool_password = env_val;
    std::free(env_val);
#else
    const char* env_val = std::getenv(cfg.pool_password_env.c_str());
    if (env_val == nullptr || env_val[0] == '\0') {
        logger.warn("config", cfg.node_id, "pool.password_env is set but environment variable is missing", cfg.pool_password_env);
        return;
    }

    cfg.pool_password = env_val;
#endif
    logger.info("config", cfg.node_id, "Loaded pool password from environment variable", cfg.pool_password_env);
}

bool validate_config(const MinerConfig& cfg, std::string& error_message) {
    if (cfg.thread_count == 0U) {
        error_message = "hashing.threads must be greater than 0";
        return false;
    }
    if (cfg.report_interval_ms == 0U) {
        error_message = "hashing.report_interval_ms must be greater than 0";
        return false;
    }

    if (!cfg.pool_enabled) {
        return true;
    }

    if (cfg.pool_host.empty()) {
        error_message = "pool.host must be set when pool.enabled=true";
        return false;
    }
    if (cfg.pool_port == 0U || cfg.pool_port > 65535U) {
        error_message = "pool.port must be in range 1..65535 when pool.enabled=true";
        return false;
    }
    if (pool_username(cfg).empty()) {
        error_message = "pool.username (or payout_address+worker_id) must be set when pool.enabled=true";
        return false;
    }
    if (pool_username(cfg).find("REPLACE_WITH") != std::string::npos) {
        error_message = "pool.username contains placeholder value; replace before running with pool.enabled=true";
        return false;
    }
    if (cfg.pool_password.empty()) {
        error_message = "pool.password is empty; set pool.password or pool.password_env";
        return false;
    }
    if (cfg.pool_password.find("REPLACE_WITH") != std::string::npos) {
        error_message = "pool.password contains placeholder value; replace before running with pool.enabled=true";
        return false;
    }
    if (cfg.pool_notify_timeout_sec == 0U) {
        error_message = "pool.notify_timeout_sec must be greater than 0";
        return false;
    }
    if (cfg.pool_reconnect_initial_sec == 0U || cfg.pool_reconnect_max_sec == 0U) {
        error_message = "pool reconnect backoff values must be greater than 0";
        return false;
    }
    if (cfg.pool_reconnect_max_sec < cfg.pool_reconnect_initial_sec) {
        error_message = "pool.reconnect_max_sec must be greater than or equal to pool.reconnect_initial_sec";
        return false;
    }

    return true;
}
