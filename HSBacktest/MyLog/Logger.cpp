#include "Logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/async.h"

namespace HSBacktest {

    Logger& Logger::getInstance() {
        static Logger instance;
        return instance;
    }

    void Logger::init(const std::string& log_path,
        spdlog::level::level_enum level,
        size_t max_file_size,
        size_t max_files) {
        try {
            // 1. 设置异步模式（高性能关键）
            // 队列大小 8192，单线程后台写入
            spdlog::init_thread_pool(8192, 1);

            // 2. 创建 Sinks 
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(level);
            // 格式说明：[时间] [日志级别] [线程ID] [源文件:行号] 内容
            console_sink->set_pattern("%Y-%m-%d %H:%M:%S.%e | %^%l%$ | %t | %s:%# | %v");

            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_path, max_file_size, max_files);
            file_sink->set_level(level);
            file_sink->set_pattern("%Y-%m-%d %H:%M:%S.%e | %l | %t | %s:%# | %v");

            // 3. 组合 Logger
            std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
            _logger = std::make_shared<spdlog::async_logger>(
                "multi_sink", sinks.begin(), sinks.end(), spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);

            // 4. 注册并设置
            _logger->set_level(level);
            spdlog::register_logger(_logger);
            spdlog::set_default_logger(_logger);

            // 遇到 flush 级别及以上的日志立即刷盘
            _logger->flush_on(spdlog::level::err);

            // 定期每3秒刷一次盘
            spdlog::flush_every(std::chrono::seconds(3));

        }
        catch (const spdlog::spdlog_ex& ex) {
            printf("Log initialization failed: %s\n", ex.what());
        }
    }

} // namespace HSBacktest