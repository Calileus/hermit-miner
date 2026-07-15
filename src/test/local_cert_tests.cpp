#include <gtest/gtest.h>

#include "lib/miner_cli.h"
#include "lib/miner_config.h"
#include "lib/logger.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

std::string quote(const fs::path& p) {
    return std::string("\"") + p.string() + "\"";
}

fs::path source_root() {
    return fs::path(IMINE_SOURCE_DIR);
}

fs::path find_binary(const std::string& name_no_ext) {
#ifdef _WIN32
    const std::string exe = name_no_ext + ".exe";
#else
    const std::string exe = name_no_ext;
#endif
    const fs::path root = source_root();
    const std::array<fs::path, 4> candidates = {
        root / "build" / "Release" / exe,
        root / "build" / "RelWithDebInfo" / exe,
        root / "build" / "Debug" / exe,
        root / "build" / exe
    };

    for (const auto& p : candidates) {
        if (fs::exists(p)) {
            return p;
        }
    }
    return {};
}

std::string read_all(const fs::path& p) {
    std::ifstream in(p);
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

std::size_t count_occurrences(const std::string& text, const std::string& token) {
    if (token.empty()) {
        return 0;
    }
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        ++count;
        pos += token.size();
    }
    return count;
}

bool replace_once(std::string& text, const std::string& from, const std::string& to) {
    const std::size_t pos = text.find(from);
    if (pos == std::string::npos) {
        return false;
    }
    text.replace(pos, from.size(), to);
    return true;
}

std::string make_test_config_json() {
        return R"({
    "node_id": "UT-1",
    "worker_id": "ut1",
    "payout_address": "wallet-ut",
    "pool": {
        "enabled": true,
        "host": "127.0.0.1",
        "port": 3347,
        "username": "wallet-ut.ut1",
        "password": "x",
        "notify_timeout_sec": 30,
        "max_cycles": 3,
        "reconnect_initial_sec": 1,
        "reconnect_max_sec": 8
    },
    "hashing": {
        "prefix": "hello-bitcoin",
        "difficulty_bits": 20,
        "threads": 2,
        "report_interval_ms": 250
    },
    "logging": {
        "output": "logs/miner-ut.log"
    }
})";
}

fs::path write_temp_config(const std::string& name, const std::string& content) {
        const fs::path out_path = source_root() / "logs" / name;
        fs::create_directories(out_path.parent_path());
        std::ofstream out(out_path);
        out << content;
        return out_path;
}

#ifdef _WIN32
using SocketType = SOCKET;
constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#else
using SocketType = int;
constexpr SocketType kInvalidSocket = -1;
#endif

void close_socket(SocketType fd) {
#ifdef _WIN32
    if (fd != kInvalidSocket) {
        closesocket(fd);
    }
#else
    if (fd != kInvalidSocket) {
        close(fd);
    }
#endif
}

bool send_line(SocketType fd, const std::string& line) {
    const std::string out = line + "\n";
    const char* ptr = out.c_str();
    std::size_t left = out.size();
    while (left > 0) {
#ifdef _WIN32
        const int sent = send(fd, ptr, static_cast<int>(left), 0);
#else
        const ssize_t sent = send(fd, ptr, left, 0);
#endif
        if (sent <= 0) {
            return false;
        }
        ptr += sent;
        left -= static_cast<std::size_t>(sent);
    }
    return true;
}

bool recv_line(SocketType fd, std::string& out) {
    out.clear();
    char ch = 0;
    while (true) {
#ifdef _WIN32
        const int rc = recv(fd, &ch, 1, 0);
#else
        const ssize_t rc = recv(fd, &ch, 1, 0);
#endif
        if (rc <= 0) {
            return false;
        }
        if (ch == '\n') {
            return true;
        }
        if (ch != '\r') {
            out.push_back(ch);
        }
    }
}

} // namespace

TEST(LocalCert, ConfigDefaultsInfiniteMode) {
    const std::string cfg = read_all(source_root() / "config" / "miner-local-stratum.json");
    ASSERT_NE(cfg.find("\"max_cycles\": 0"), std::string::npos);
}

TEST(LocalCert, FakePoolRawHandshakeFlow) {
    const fs::path fake_pool = find_binary("i_mine_fake_pool");
    ASSERT_FALSE(fake_pool.empty()) << "i_mine_fake_pool not found";

#ifdef _WIN32
    WSADATA wsa{};
    ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &wsa), 0);
#endif

    const fs::path fake_pool_log = source_root() / "logs" / "fake-pool-test.log";
    fs::create_directories(fake_pool_log.parent_path());

    auto server_future = std::async(std::launch::async, [fake_pool, fake_pool_log]() {
        const std::string cmd = fake_pool.string() + " 3347 > " + fake_pool_log.string() + " 2>&1";
        return std::system(cmd.c_str());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    SocketType fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    ASSERT_NE(fd, kInvalidSocket);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(3347);
    ASSERT_EQ(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr), 1);
    ASSERT_EQ(connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

    ASSERT_TRUE(send_line(fd, "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"local-cert\"]}"));
    std::string line;
    ASSERT_TRUE(recv_line(fd, line));
    EXPECT_NE(line.find("\"id\":1"), std::string::npos);

    ASSERT_TRUE(send_line(fd, "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"wallet.worker\",\"x\"]}"));
    ASSERT_TRUE(recv_line(fd, line));
    EXPECT_NE(line.find("\"id\":2"), std::string::npos);

    ASSERT_TRUE(recv_line(fd, line));
    EXPECT_NE(line.find("\"method\":\"mining.notify\""), std::string::npos);

    ASSERT_TRUE(send_line(fd, "{\"id\":3,\"method\":\"mining.submit\",\"params\":[\"wallet.worker\",\"job-1\",\"00000001\"]}"));
    ASSERT_TRUE(recv_line(fd, line));
    EXPECT_NE(line.find("\"id\":3"), std::string::npos);

    close_socket(fd);

    const auto wait_rc = server_future.wait_for(std::chrono::seconds(5));
    ASSERT_EQ(wait_rc, std::future_status::ready);
    EXPECT_EQ(server_future.get(), 0);

#ifdef _WIN32
    WSACleanup();
#endif
}

TEST(LocalCert, MinerMultiCycleSessionAgainstFakePool) {
    const fs::path fake_pool = find_binary("i_mine_fake_pool");
    const fs::path miner = find_binary("i_mine");
    ASSERT_FALSE(fake_pool.empty()) << "i_mine_fake_pool not found";
    ASSERT_FALSE(miner.empty()) << "i_mine not found";

    const fs::path fake_pool_log = source_root() / "logs" / "fake-pool-integration.log";
    const fs::path miner_log = source_root() / "logs" / "miner-integration.log";
    fs::create_directories(miner_log.parent_path());

    auto server_future = std::async(std::launch::async, [fake_pool, fake_pool_log]() {
        const std::string cmd = fake_pool.string() + " 3347 > " + fake_pool_log.string() + " 2>&1";
        return std::system(cmd.c_str());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    const fs::path test_cfg = source_root() / "config" / "miner-local-stratum-test.json";
    const std::string miner_cmd = miner.string() + " --config " + test_cfg.string() + " > " + miner_log.string() + " 2>&1";
    const int miner_rc = std::system(miner_cmd.c_str());
    EXPECT_EQ(miner_rc, 0);

    const auto wait_rc = server_future.wait_for(std::chrono::seconds(10));
    ASSERT_EQ(wait_rc, std::future_status::ready);
    EXPECT_EQ(server_future.get(), 0);

    const std::string out = read_all(miner_log);
    EXPECT_NE(out.find("Subscribe OK"), std::string::npos);
    EXPECT_NE(out.find("Authorize OK"), std::string::npos);
    EXPECT_NE(out.find("Stratum session reached cycle limit"), std::string::npos);
    EXPECT_NE(out.find("Shutdown summary"), std::string::npos);
    EXPECT_NE(out.find("accepted_count=3"), std::string::npos);
    EXPECT_NE(out.find("last_job_id=job-3"), std::string::npos);

    const std::size_t share_accepts = count_occurrences(out, "Share accepted");
    EXPECT_GE(share_accepts, 3U);
}

TEST(LocalCert, MinerSupportsHostnamePoolHost) {
    const fs::path fake_pool = find_binary("i_mine_fake_pool");
    const fs::path miner = find_binary("i_mine");
    ASSERT_FALSE(fake_pool.empty()) << "i_mine_fake_pool not found";
    ASSERT_FALSE(miner.empty()) << "i_mine not found";

    const fs::path fake_pool_log = source_root() / "logs" / "fake-pool-hostname.log";
    const fs::path miner_log = source_root() / "logs" / "miner-hostname.log";
    const fs::path base_cfg = source_root() / "config" / "miner-local-stratum-test.json";
    const fs::path hostname_cfg = source_root() / "logs" / "miner-local-stratum-hostname.json";
    fs::create_directories(miner_log.parent_path());

    std::string cfg_text = read_all(base_cfg);
    ASSERT_FALSE(cfg_text.empty());
    ASSERT_TRUE(replace_once(cfg_text, "\"host\": \"127.0.0.1\"", "\"host\": \"localhost\""));
    ASSERT_TRUE(replace_once(cfg_text, "\"output\": \"logs/miner-offline-stratum-test.log\"", "\"output\": \"logs/miner-hostname-test.log\""));
    {
        std::ofstream out_cfg(hostname_cfg);
        ASSERT_TRUE(out_cfg.is_open());
        out_cfg << cfg_text;
    }

    auto server_future = std::async(std::launch::async, [fake_pool, fake_pool_log]() {
        const std::string cmd = fake_pool.string() + " 3347 > " + fake_pool_log.string() + " 2>&1";
        return std::system(cmd.c_str());
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(350));

    const std::string miner_cmd = miner.string() + " --config " + hostname_cfg.string() + " > " + miner_log.string() + " 2>&1";
    const int miner_rc = std::system(miner_cmd.c_str());
    EXPECT_EQ(miner_rc, 0);

    const auto wait_rc = server_future.wait_for(std::chrono::seconds(10));
    ASSERT_EQ(wait_rc, std::future_status::ready);
    EXPECT_EQ(server_future.get(), 0);

    const std::string out = read_all(miner_log);
    EXPECT_NE(out.find("Connected to pool"), std::string::npos);
    EXPECT_NE(out.find("localhost:3347"), std::string::npos);
    EXPECT_NE(out.find("Share accepted"), std::string::npos);
}

TEST(LocalCert, ConfigParserRejectsMalformedJson) {
    const fs::path cfg_path = write_temp_config("malformed-config.json", "{\"pool\": {\"enabled\": true");

    MinerConfig cfg;
    EXPECT_FALSE(load_config(cfg_path.string(), cfg));
}

TEST(LocalCert, CliParsesOverridesFromConfigAndFlags) {
    const fs::path cfg_path = write_temp_config("cli-override-config.json", make_test_config_json());

    MinerConfig cfg;
    ASSERT_TRUE(load_config(cfg_path.string(), cfg));
    std::string config_path = cfg_path.string();

    std::vector<std::string> args_storage = {
        "i_mine",
        "--config", cfg_path.string(),
        "--threads", "4",
        "--bits", "21",
        "--prefix", "cli-prefix",
        "--report-ms", "999"
    };

    std::vector<char*> argv;
    argv.reserve(args_storage.size());
    for (std::string& arg : args_storage) {
        argv.push_back(arg.data());
    }

    ASSERT_EQ(parse_args(static_cast<int>(argv.size()), argv.data(), config_path, cfg), CliParseResult::Ok);
    EXPECT_EQ(config_path, cfg_path.string());
    EXPECT_EQ(cfg.thread_count, 4U);
    EXPECT_EQ(cfg.difficulty_bits, 21U);
    EXPECT_EQ(cfg.prefix, "cli-prefix");
    EXPECT_EQ(cfg.report_interval_ms, 999U);
}

TEST(LocalCert, CliRejectsUnknownOption) {
    MinerConfig cfg;
    std::string config_path = "config/miner-local-stratum.json";

    std::vector<std::string> args_storage = {
        "i_mine",
        "--unknown", "x"
    };

    std::vector<char*> argv;
    argv.reserve(args_storage.size());
    for (std::string& arg : args_storage) {
        argv.push_back(arg.data());
    }

    EXPECT_EQ(parse_args(static_cast<int>(argv.size()), argv.data(), config_path, cfg), CliParseResult::Error);
}

TEST(LocalCert, CliHelpReturnsHelpShown) {
    MinerConfig cfg;
    std::string config_path = "config/miner-local-stratum.json";

    std::vector<std::string> args_storage = {
        "i_mine",
        "--help"
    };

    std::vector<char*> argv;
    argv.reserve(args_storage.size());
    for (std::string& arg : args_storage) {
        argv.push_back(arg.data());
    }

    EXPECT_EQ(parse_args(static_cast<int>(argv.size()), argv.data(), config_path, cfg), CliParseResult::HelpShown);
}

TEST(LocalCert, CliRejectsMissingOptionValue) {
    MinerConfig cfg;
    std::string config_path = "config/miner-local-stratum.json";

    std::vector<std::string> args_storage = {
        "i_mine",
        "--threads"
    };

    std::vector<char*> argv;
    argv.reserve(args_storage.size());
    for (std::string& arg : args_storage) {
        argv.push_back(arg.data());
    }

    EXPECT_EQ(parse_args(static_cast<int>(argv.size()), argv.data(), config_path, cfg), CliParseResult::Error);
}

TEST(LocalCert, ConfigValidationRejectsEnabledPoolWithoutHost) {
    MinerConfig cfg;
    cfg.pool_enabled = true;
    cfg.pool_host.clear();
    cfg.pool_password = "x";
    cfg.pool_notify_timeout_sec = 30;
    cfg.pool_reconnect_initial_sec = 1;
    cfg.pool_reconnect_max_sec = 8;

    std::string error;
    EXPECT_FALSE(validate_config(cfg, error));
    EXPECT_NE(error.find("pool.host"), std::string::npos);
}

TEST(LocalCert, ConfigValidationAcceptsDisabledPoolMinimalConfig) {
    MinerConfig cfg;
    cfg.pool_enabled = false;
    cfg.thread_count = 1;
    cfg.report_interval_ms = 100;

    std::string error;
    EXPECT_TRUE(validate_config(cfg, error));
}

TEST(LocalCert, ConfigParserSupportsFlatLegacyKeys) {
    const std::string flat_json = R"({
  "node_id": "LEGACY-1",
  "worker_id": "legacy01",
  "payout_address": "legacy-wallet",
  "enabled": true,
  "host": "localhost",
  "port": 3333,
  "username": "legacy-wallet.legacy01",
  "password": "legacy-pass",
  "notify_timeout_sec": 12,
  "max_cycles": 5,
  "reconnect_initial_sec": 1,
  "reconnect_max_sec": 4,
  "prefix": "legacy-prefix",
  "difficulty_bits": 19,
  "threads": 3,
  "report_interval_ms": 222,
  "output": "logs/legacy.log"
})";

    const fs::path cfg_path = write_temp_config("legacy-flat-config.json", flat_json);
    MinerConfig cfg;
    ASSERT_TRUE(load_config(cfg_path.string(), cfg));

    EXPECT_EQ(cfg.node_id, "LEGACY-1");
    EXPECT_EQ(cfg.worker_id, "legacy01");
    EXPECT_EQ(cfg.pool_host, "localhost");
    EXPECT_EQ(cfg.pool_port, 3333U);
    EXPECT_EQ(cfg.pool_username, "legacy-wallet.legacy01");
    EXPECT_EQ(cfg.pool_password, "legacy-pass");
    EXPECT_EQ(cfg.pool_notify_timeout_sec, 12U);
    EXPECT_EQ(cfg.pool_max_cycles, 5U);
    EXPECT_EQ(cfg.pool_reconnect_initial_sec, 1U);
    EXPECT_EQ(cfg.pool_reconnect_max_sec, 4U);
    EXPECT_EQ(cfg.prefix, "legacy-prefix");
    EXPECT_EQ(cfg.difficulty_bits, 19U);
    EXPECT_EQ(cfg.thread_count, 3U);
    EXPECT_EQ(cfg.report_interval_ms, 222U);
    EXPECT_EQ(cfg.log_output, "logs/legacy.log");
}

TEST(LocalCert, LoggerRedactsSensitiveValues) {
    const fs::path log_path = source_root() / "logs" / "redaction-unit.log";
    fs::create_directories(log_path.parent_path());
    if (fs::exists(log_path)) {
        fs::remove(log_path);
    }

    mining::Logger logger(log_path.string(), mining::LogLevel::INFO);
    logger.info("security", "UT-SEC", "password=secret123 username=alice token=tkn123", "payout_address=walletXYZ auth_token=abc");
    logger.info("security", "UT-SEC", "{\"password\":\"jsonSecret\",\"username\":\"bob\"}");

    const std::string out = read_all(log_path);
    EXPECT_FALSE(out.empty());
    EXPECT_EQ(out.find("secret123"), std::string::npos);
    EXPECT_EQ(out.find("alice"), std::string::npos);
    EXPECT_EQ(out.find("tkn123"), std::string::npos);
    EXPECT_EQ(out.find("walletXYZ"), std::string::npos);
    EXPECT_EQ(out.find("jsonSecret"), std::string::npos);
    EXPECT_NE(out.find("***"), std::string::npos);
}
