#include <gtest/gtest.h>

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
