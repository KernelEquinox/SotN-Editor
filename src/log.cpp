#include <stdexcept>
#include <cstdarg>
#include "log.h"
#include "utils.h"



// Default to info level logging
uint Log::level = LOG_INFO;



/**
 * Logs debug messages to the console.
 *
 * @param fmt: Format string
 * @param ...: List of arguments to include in the output string
 *
 */
void Log::Debug(const char* fmt, ...) {

    // Bail if log level isn't debug or higher
    if (level < LOG_DEBUG) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    std::string output = Utils::FormatStringArgs(fmt, args);
    printf("[DEBUG]    %s", output.c_str());
    va_end(args);
}



/**
 * Logs informational messages to the console.
 *
 * @param fmt: Format string
 * @param ...: List of arguments to include in the output string
 *
 */
void Log::Info(const char* fmt, ...) {

    // Bail if log level isn't info or higher
    if (level < LOG_INFO) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    std::string output = Utils::FormatStringArgs(fmt, args);
    printf("[INFO]     %s", output.c_str());
    va_end(args);
}



/**
 * Logs warning messages to the console.
 *
 * @param fmt: Format string
 * @param ...: List of arguments to include in the output string
 *
 */
void Log::Warn(const char* fmt, ...) {

    // Bail if log level isn't warn or higher
    if (level < LOG_WARN) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    std::string output = Utils::FormatStringArgs(fmt, args);
    printf("[WARN]     %s", output.c_str());
    va_end(args);
}



/**
 * Logs error messages to the console.
 *
 * @param fmt: Format string
 * @param ...: List of arguments to include in the output string
 *
 */
void Log::Error(const char* fmt, ...) {

    // Bail if log level isn't error or higher
    if (level < LOG_ERROR) {
        return;
    }

    va_list args;
    va_start(args, fmt);
    std::string output = Utils::FormatStringArgs(fmt, args);
    printf("[ERROR]    %s", output.c_str());
    va_end(args);
}
