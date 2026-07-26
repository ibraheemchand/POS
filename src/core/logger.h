#pragma once
#include <QString>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace pos {
enum class LogLevel { Trace, Debug, Info, Warning, Error, Critical };
class Logger final {
public:
    static Logger& instance();
    void configure(const std::filesystem::path& directory, LogLevel minimum = LogLevel::Info);
    void log(LogLevel level, const QString& message);
    void logException(const QString& context, const std::exception& exception);
    void flush();
private:
    Logger() = default;
    void rotateIfNeeded();
    std::mutex mutex_;
    std::filesystem::path directory_;
    std::ofstream file_;
    QString activeDate_;
    LogLevel minimum_{LogLevel::Info};
};
}
#define POS_LOG_TRACE(message) ::pos::Logger::instance().log(::pos::LogLevel::Trace, (message))
#define POS_LOG_DEBUG(message) ::pos::Logger::instance().log(::pos::LogLevel::Debug, (message))
#define POS_LOG_INFO(message) ::pos::Logger::instance().log(::pos::LogLevel::Info, (message))
#define POS_LOG_WARNING(message) ::pos::Logger::instance().log(::pos::LogLevel::Warning, (message))
#define POS_LOG_ERROR(message) ::pos::Logger::instance().log(::pos::LogLevel::Error, (message))
#define POS_LOG_CRITICAL(message) ::pos::Logger::instance().log(::pos::LogLevel::Critical, (message))
