#pragma once

#include <memory>
#include <string>

// 先定义日志级别，再包含 spdlog
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE 
#endif

#include "spdlog/spdlog.h"
#include "fmt/format.h"

namespace HSBacktest {

    class Logger {
    public:
        static Logger& getInstance();

        // 初始化日志，支持自定义日志路径、日志等级、最大文件大小、文件个数
        void init(const std::string& log_path,
            spdlog::level::level_enum level = spdlog::level::info,
            size_t max_file_size = 10 * 1024 * 1024,
            size_t max_files = 5);

        auto getLogger() { return _logger; }

    private:
        Logger() = default;
        std::shared_ptr<spdlog::logger> _logger;
    };

} // namespace HSBacktest

// --- 宏封装层 ---

// 使用 ##__VA_ARGS__ 处理空参数情况 (GCC/Clang)
// 使用 SPDLOG_LOGGER_CALL 确保能捕获文件名和行号

#define LOG_TRACE(...) SPDLOG_LOGGER_CALL(HSBacktest::Logger::getInstance().getLogger(), spdlog::level::trace, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(HSBacktest::Logger::getInstance().getLogger(), spdlog::level::debug, __VA_ARGS__)
#define LOG_INFO(...)  SPDLOG_LOGGER_CALL(HSBacktest::Logger::getInstance().getLogger(), spdlog::level::info, __VA_ARGS__)
#define LOG_WARN(...)  SPDLOG_LOGGER_CALL(HSBacktest::Logger::getInstance().getLogger(), spdlog::level::warn, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(HSBacktest::Logger::getInstance().getLogger(), spdlog::level::err, __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CALL(HSBacktest::Logger::getInstance().getLogger(), spdlog::level::critical, __VA_ARGS__)

// 额外的便捷宏：仅在满足条件时打印
#define LOG_IF(level, cond, ...) if (cond) LOG_##level(__VA_ARGS__)
