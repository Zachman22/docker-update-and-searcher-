#include "utils/Logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace utils {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger()
    : currentLevel_(LogLevel::Info)
    , consoleOutputEnabled_(true)
    , fileOutputEnabled_(false)
{}

Logger::~Logger() {
    if (logFile_ && logFile_->is_open()) {
        logFile_->close();
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::Debug, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::Info, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::Warning, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

void Logger::critical(const std::string& message) {
    log(LogLevel::Critical, message);
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel_ = level;
}

void Logger::setLogFile(const std::string& filePath) {
    logFilePath_ = filePath;
    logFile_ = std::make_unique<std::ofstream>(filePath, std::ios::app);
}

void Logger::enableConsoleOutput(bool enable) {
    consoleOutputEnabled_ = enable;
}

void Logger::enableFileOutput(bool enable) {
    fileOutputEnabled_ = enable;
}

void Logger::flush() {
    if (logFile_ && logFile_->is_open()) {
        logFile_->flush();
    }
}

void Logger::rotateLog() {
    // TODO: Implement log rotation
}

void Logger::clearLog() {
    if (logFile_ && logFile_->is_open()) {
        logFile_->close();
        logFile_ = std::make_unique<std::ofstream>(logFilePath_, std::ios::trunc);
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel_) {
        return;
    }

    std::string timestamp = getCurrentTimestamp();
    std::string levelStr = getLevelString(level);
    std::string logMessage = "[" + timestamp + "] [" + levelStr + "] " + message;

    if (consoleOutputEnabled_) {
        std::cout << logMessage << std::endl;
    }

    if (fileOutputEnabled_ && logFile_ && logFile_->is_open()) {
        (*logFile_) << logMessage << std::endl;
        logFile_->flush();
    }
}

std::string Logger::getLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

} // namespace utils
