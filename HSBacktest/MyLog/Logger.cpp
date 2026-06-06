#include "Logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/async.h"

#include <mutex>
#include <filesystem>

namespace HSBacktest {

    Logger& Logger::getInstance() {
        static Logger instance;
        return instance;
    }

    void Logger::init(const std::string& log_basename,
        spdlog::level::level_enum level,
        size_t max_file_size,
        size_t max_files) {
        // ==========================================
        // 0. 幂等保护：重复 init 先卸旧 logger
        // ==========================================
        if (_logger) {
            spdlog::drop("hsbacktest");
            _logger.reset();
        }

        // ==========================================
        // 1. 线程池：全进程只初始化一次
        // ==========================================
        static std::once_flag thread_pool_flag;
        std::call_once(thread_pool_flag, []() {
            spdlog::init_thread_pool(8192, 1);
        });

        // ==========================================
        // 2. 控制台 sink（始终创建）
        // ==========================================
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        console_sink->set_level(level);
        console_sink->set_pattern("%Y-%m-%d %H:%M:%S.%e | %^%l%$ | %t | %s:%# | %v");

        std::vector<spdlog::sink_ptr> sinks{ console_sink };

        // ==========================================
        // 3. 文件 sink（尽力创建，失败回退到纯控制台）
        // ==========================================
        try {
            // 自动补全路径：logs/ 目录 + .log 后缀
            std::filesystem::path file_path(log_basename);

            // 如果用户没给目录，默认放在 logs/ 下
            if (!file_path.has_parent_path() || file_path.parent_path().empty()) {
                std::filesystem::create_directories("logs");
                file_path = std::filesystem::path("logs") / file_path.filename();
            } else {
                std::filesystem::create_directories(file_path.parent_path());
            }

            // 如果没给扩展名，默认 .log
            if (!file_path.has_extension()) {
                file_path += ".log";
            }

            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                file_path.string(), max_file_size, max_files);
            file_sink->set_level(level);
            file_sink->set_pattern("%Y-%m-%d %H:%M:%S.%e | %l | %t | %s:%# | %v");

            sinks.push_back(file_sink);

            printf("[Logger] 日志文件: %s\n", file_path.string().c_str());
        }
        catch (const std::exception& ex) {
            printf("[Logger] 文件 sink 创建失败: %s —— 回退到仅控制台输出\n", ex.what());
        }

        // ==========================================
        // 4. 组装 Logger
        // ==========================================
        _logger = std::make_shared<spdlog::async_logger>(
            "hsbacktest", sinks.begin(), sinks.end(), spdlog::thread_pool(),
            spdlog::async_overflow_policy::block);

        _logger->set_level(level);
        spdlog::register_logger(_logger);
        spdlog::set_default_logger(_logger);

        _logger->flush_on(spdlog::level::err);

        // 全局定期刷盘（仅一次）
        static std::once_flag flush_flag;
        std::call_once(flush_flag, []() {
            spdlog::flush_every(std::chrono::seconds(3));
        });
    }

} // namespace HSBacktest
