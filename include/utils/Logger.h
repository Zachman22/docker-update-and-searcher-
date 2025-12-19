#pragma once

#include <string>
#include <memory>
#include <fstream>

namespace utils {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
    Critical
};

class Logger {
public:
    static Logger& getInstance();

    // Logging methods
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void critical(const std::string& message);

    // Configuration
    void setLogLevel(LogLevel level);
    void setLogFile(const std::string& filePath);
    void enableConsoleOutput(bool enable);
    void enableFileOutput(bool enable);

    // Log management
    void flush();
    void rotateLog();
    void clearLog();

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& message);
    std::string getLevelString(LogLevel level);
    std::string getCurrentTimestamp();

    LogLevel currentLevel_;
    std::string logFilePath_;
    bool consoleOutputEnabled_;
    bool fileOutputEnabled_;
    std::unique_ptr<std::ofstream> logFile_;
};

// Convenience macros
#define LOG_DEBUG(msg) utils::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) utils::Logger::getInstance().info(msg)
#define LOG_WARNING(msg) utils::Logger::getInstance().warning(msg)
#define LOG_ERROR(msg) utils::Logger::getInstance().error(msg)
#define LOG_CRITICAL(msg) utils::Logger::getInstance().critical(msg)

} // namespace utils
