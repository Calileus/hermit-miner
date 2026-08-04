#include "miner_runtime.h"

#include "job.h"
#include "logger.h"
#include "stratum.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace
{

    struct SessionStats
    {
        std::uint64_t jobs_received = 0;
        std::uint64_t shares_found = 0;
        std::uint64_t submits_attempted = 0;
        std::uint64_t submit_failures = 0;
        std::uint64_t accepted_count = 0;
        std::uint64_t reconnect_events = 0;
        std::uint64_t session_failures = 0;
        int last_failure_rc = 0;
        std::string last_job_id = "none";
        std::chrono::steady_clock::time_point started_at = std::chrono::steady_clock::now();
    };

    std::string json_escape(const std::string &input)
    {
        std::string out;
        out.reserve(input.size() + 8);
        for (char ch : input)
        {
            switch (ch)
            {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(ch);
                break;
            }
        }
        return out;
    }

    void write_health_snapshot(
        const MinerConfig &cfg,
        const SessionStats &stats,
        const std::string &readiness_status,
        std::uint64_t duration_sec,
        double accepted_per_min,
        mining::Logger &logger)
    {
        if (cfg.health_output.empty())
        {
            return;
        }

        const std::filesystem::path out_path(cfg.health_output);
        const std::filesystem::path parent = out_path.parent_path();
        if (!parent.empty())
        {
            std::error_code ec;
            std::filesystem::create_directories(parent, ec);
        }

        std::ofstream out(cfg.health_output, std::ios_base::trunc);
        if (!out.is_open())
        {
            logger.warn("health", cfg.node_id, "Failed to write health snapshot", cfg.health_output);
            return;
        }

        out << "{"
            << "\"node_id\":\"" << json_escape(cfg.node_id) << "\","
            << "\"status\":\"" << json_escape(readiness_status) << "\","
            << "\"accepted_count\":" << stats.accepted_count << ","
            << "\"jobs_received\":" << stats.jobs_received << ","
            << "\"shares_found\":" << stats.shares_found << ","
            << "\"submits_attempted\":" << stats.submits_attempted << ","
            << "\"submit_failures\":" << stats.submit_failures << ","
            << "\"reconnect_events\":" << stats.reconnect_events << ","
            << "\"session_failures\":" << stats.session_failures << ","
            << "\"session_duration_sec\":" << duration_sec << ","
            << "\"accepted_per_min\":" << std::fixed << std::setprecision(2) << accepted_per_min << ","
            << "\"last_job_id\":\"" << json_escape(stats.last_job_id) << "\""
            << "}";
    }

    double compute_accepted_per_min(const SessionStats &stats, std::uint64_t duration_sec)
    {
        const double minutes = duration_sec > 0U ? static_cast<double>(duration_sec) / 60.0 : 0.0;
        return minutes > 0.0
                   ? static_cast<double>(stats.accepted_count) / minutes
                   : static_cast<double>(stats.accepted_count);
    }

    std::string compute_readiness_status(const SessionStats &stats)
    {
        if (stats.accepted_count >= 3U && stats.submit_failures == 0U && stats.session_failures == 0U)
        {
            return "ready";
        }
        if (stats.accepted_count > 0U)
        {
            return "degraded";
        }
        return "not_ready";
    }

    void maybe_write_health_snapshot_incremental(const MinerConfig &cfg, const SessionStats &stats, mining::Logger &logger)
    {
        if (!cfg.health_emit_each_cycle)
        {
            return;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto duration_sec = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(now - stats.started_at).count());
        const double accepted_per_min = compute_accepted_per_min(stats, duration_sec);
        const std::string readiness_status = compute_readiness_status(stats);

        write_health_snapshot(cfg, stats, readiness_status, duration_sec, accepted_per_min, logger);
    }

    bool start_stratum_session(const MinerConfig &cfg, mining::Logger &logger, mining::StratumClient &client)
    {
        const std::string username = pool_username(cfg);

        if (!client.connect_to_pool())
        {
            return false;
        }
        if (!client.subscribe("hermit_miner/stratum-offline-ready"))
        {
            return false;
        }
        if (!client.authorize(username, cfg.pool_password))
        {
            return false;
        }

        logger.info("stratum", cfg.node_id, "Stratum session started", "user=" + username);
        return true;
    }

    int run_stratum_session(const MinerConfig &cfg, mining::Logger &logger, std::uint32_t &cycles_done, SessionStats &stats, const std::atomic<bool> &stop_requested)
    {
        mining::StratumClient client(
            cfg.pool_host,
            static_cast<std::uint16_t>(cfg.pool_port),
            logger,
            cfg.node_id);

        if (!start_stratum_session(cfg, logger, client))
        {
            return 4;
        }

        const std::string username = pool_username(cfg);
        while (!stop_requested.load(std::memory_order_relaxed) && (cfg.pool_max_cycles == 0U || cycles_done < cfg.pool_max_cycles))
        {
            mining::StratumJob job;
            if (!client.wait_for_job(job, cfg.pool_notify_timeout_sec, &stop_requested))
            {
                if (stop_requested.load(std::memory_order_relaxed))
                {
                    return 0;
                }
                logger.warn("stratum", cfg.node_id, "Session lost while waiting for job; reconnect required");
                return 6;
            }

            ++stats.jobs_received;

            MinerConfig mining_cfg = cfg;
            if (!job.prefix.empty())
            {
                mining_cfg.prefix = job.prefix;
            }
            mining_cfg.difficulty_bits = job.difficulty_bits;

            std::ostringstream job_ctx;
            job_ctx << "job_id=" << job.job_id
                    << " prefix=" << mining_cfg.prefix
                    << " bits=" << mining_cfg.difficulty_bits
                    << " clean_job=" << (job.clean_job ? "true" : "false");
            logger.info("stratum", cfg.node_id, "Mining on Stratum job", job_ctx.str());

            const MiningResult result = mine(mining_cfg, logger, &stop_requested);
            if (result.aborted)
            {
                return 0;
            }
            if (!result.found)
            {
                logger.warn("stratum", cfg.node_id, "No valid nonce found for job", job.job_id);
                continue;
            }

            ++stats.shares_found;
            ++stats.submits_attempted;

            if (!client.submit_share(username, job.job_id, result.nonce))
            {
                ++stats.submit_failures;
                logger.warn("stratum", cfg.node_id, "Submit failed; reconnect required", job.job_id);
                return 8;
            }

            ++cycles_done;
            ++stats.accepted_count;
            stats.last_job_id = job.job_id;
            maybe_write_health_snapshot_incremental(cfg, stats, logger);
            logger.info("stratum", cfg.node_id, "Stratum cycle completed", job.job_id);
        }

        logger.info("stratum", cfg.node_id, "Stratum session reached cycle limit", "cycles=" + std::to_string(cycles_done));
        return 0;
    }

} // namespace

MiningResult mine(const MinerConfig &cfg, mining::Logger &logger, const std::atomic<bool> *stop_requested)
{
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> total_hashes{0U};
    std::mutex result_mutex;
    MiningResult result;

    const auto start = std::chrono::steady_clock::now();
    std::vector<std::thread> workers;
    workers.reserve(cfg.thread_count);

    for (std::uint32_t tid = 0; tid < cfg.thread_count; ++tid)
    {
        workers.emplace_back([&, tid]()
                             {
            std::vector<std::uint8_t> payload(cfg.prefix.begin(), cfg.prefix.end());
            payload.resize(payload.size() + sizeof(std::uint32_t));

            std::uint64_t local_hashes = 0U;
            for (std::uint64_t nonce64 = tid; nonce64 <= std::numeric_limits<std::uint32_t>::max(); nonce64 += cfg.thread_count) {
                if (stop_requested != nullptr && stop_requested->load(std::memory_order_relaxed)) {
                    break;
                }
                if (stop.load(std::memory_order_relaxed)) {
                    break;
                }

                const std::uint32_t nonce = static_cast<std::uint32_t>(nonce64);
                const std::size_t base = payload.size() - sizeof(std::uint32_t);
                payload[base + 0U] = static_cast<std::uint8_t>(nonce & 0xFFU);
                payload[base + 1U] = static_cast<std::uint8_t>((nonce >> 8U) & 0xFFU);
                payload[base + 2U] = static_cast<std::uint8_t>((nonce >> 16U) & 0xFFU);
                payload[base + 3U] = static_cast<std::uint8_t>((nonce >> 24U) & 0xFFU);

                const btc::Hash256 hash = btc::double_sha256(payload);
                ++local_hashes;

                if (mining::meets_difficulty(hash, cfg.difficulty_bits)) {
                    bool expected = false;
                    if (stop.compare_exchange_strong(expected, true, std::memory_order_relaxed)) {
                        std::lock_guard<std::mutex> lock(result_mutex);
                        result.found = true;
                        result.nonce = nonce;
                        result.hash = hash;
                    }
                    break;
                }
            }

            total_hashes.fetch_add(local_hashes, std::memory_order_relaxed); });
    }

    while (!stop.load(std::memory_order_relaxed))
    {
        if (stop_requested != nullptr && stop_requested->load(std::memory_order_relaxed))
        {
            stop.store(true, std::memory_order_relaxed);
            result.aborted = true;
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(cfg.report_interval_ms));
        const auto now = std::chrono::steady_clock::now();
        const double sec = std::chrono::duration<double>(now - start).count();
        const std::uint64_t hashes = total_hashes.load(std::memory_order_relaxed);
        const double hps = sec > 0.0 ? static_cast<double>(hashes) / sec : 0.0;

        std::ostringstream status;
        status << "hashes=" << hashes
               << " elapsed=" << std::fixed << std::setprecision(2) << sec << "s"
               << " h/s=" << std::setprecision(0) << hps;
        logger.info("miner", cfg.node_id, "Progress", status.str());

        if (hashes >= std::numeric_limits<std::uint32_t>::max())
        {
            stop.store(true, std::memory_order_relaxed);
            break;
        }
    }

    for (auto &t : workers)
    {
        if (t.joinable())
        {
            t.join();
        }
    }

    const auto end = std::chrono::steady_clock::now();
    result.elapsed_seconds = std::chrono::duration<double>(end - start).count();
    result.hashes_tried = total_hashes.load(std::memory_order_relaxed);
    return result;
}

int run_stratum_loop(const MinerConfig &cfg, mining::Logger &logger, const std::atomic<bool> &stop_requested)
{
    std::uint32_t cycles_done = 0;
    std::uint32_t backoff_sec = std::max(1U, cfg.pool_reconnect_initial_sec);
    const std::uint32_t backoff_max = std::max(backoff_sec, cfg.pool_reconnect_max_sec);
    SessionStats stats;
    stats.started_at = std::chrono::steady_clock::now();
    int final_rc = 0;

    while (!stop_requested.load(std::memory_order_relaxed) && (cfg.pool_max_cycles == 0U || cycles_done < cfg.pool_max_cycles))
    {
        const int rc = run_stratum_session(cfg, logger, cycles_done, stats, stop_requested);
        if (rc == 0)
        {
            backoff_sec = std::max(1U, cfg.pool_reconnect_initial_sec);
            continue;
        }

        ++stats.reconnect_events;
        ++stats.session_failures;
        stats.last_failure_rc = rc;
        maybe_write_health_snapshot_incremental(cfg, stats, logger);

        if (cfg.pool_max_reconnect_attempts > 0U && stats.session_failures >= cfg.pool_max_reconnect_attempts)
        {
            std::ostringstream exhausted;
            exhausted << "rc=" << rc
                      << " max_reconnect_attempts=" << cfg.pool_max_reconnect_attempts
                      << " cycles_done=" << cycles_done;
            logger.error("stratum", cfg.node_id, "Reconnect attempt limit reached", exhausted.str());
            final_rc = rc;
            break;
        }

        std::ostringstream ctx;
        ctx << "rc=" << rc
            << " sleep_sec=" << backoff_sec
            << " cycles_done=" << cycles_done;
        logger.warn("stratum", cfg.node_id, "Stratum cycle failed; reconnecting", ctx.str());

        if (stop_requested.load(std::memory_order_relaxed))
        {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(backoff_sec));
        backoff_sec = std::min(backoff_max, backoff_sec * 2U);
    }

    const auto now = std::chrono::steady_clock::now();
    const auto duration_sec = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(now - stats.started_at).count());
    const double accepted_per_min = compute_accepted_per_min(stats, duration_sec);
    const std::string readiness_status = compute_readiness_status(stats);

    std::ostringstream summary;
    summary << "accepted_count=" << stats.accepted_count
            << " jobs_received=" << stats.jobs_received
            << " shares_found=" << stats.shares_found
            << " submits_attempted=" << stats.submits_attempted
            << " submit_failures=" << stats.submit_failures
            << " reconnect_events=" << stats.reconnect_events
            << " session_failures=" << stats.session_failures
            << " last_failure_rc=" << stats.last_failure_rc
            << " session_duration_sec=" << duration_sec
            << " accepted_per_min=" << std::fixed << std::setprecision(2) << accepted_per_min
            << " last_job_id=" << stats.last_job_id;
    logger.info("stratum", cfg.node_id, "Shutdown summary", summary.str());

    std::ostringstream readiness;
    readiness << "status=" << readiness_status
              << " accepted_count=" << stats.accepted_count
              << " submit_failures=" << stats.submit_failures
              << " session_failures=" << stats.session_failures
              << " reconnect_events=" << stats.reconnect_events;
    logger.info("stratum", cfg.node_id, "Readiness report", readiness.str());

    write_health_snapshot(cfg, stats, readiness_status, duration_sec, accepted_per_min, logger);

    logger.info("stratum", cfg.node_id, "Stratum loop finished", "cycles=" + std::to_string(cycles_done));
    return final_rc;
}
