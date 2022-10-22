#include <stdexcept>
#include <cstdarg>
#include "log.h"



// Default to info level logging
uint Log::level = LOG_INFO;



/**
 * Formats a string for inclusion in output functionality.
 *
 * @param fmt: Format string
 * @param args: List of arguments to include in the output string
 *
 * @return Formatted string
 *
 * @throw invalid_argument: Thrown if the format arguments weren't able to be parsed.
 *
 */
std::string format_string(const char* fmt, va_list args) {

    // Get the size of the string
    int len = vsnprintf(nullptr, 0, fmt, args) + 1;

    // Check if length was invalid
    if (len <= 0) {
        va_end(args);
        throw std::invalid_argument("ERROR: Couldn't parse format arguments.");
    }

    // Allocate a new char buffer
    char* buf = (char*)calloc(len, sizeof(byte));

    // Copy args to string
    vsnprintf(buf, len, fmt, args);

    // Create string from char buffer
    std::string log_msg = std::string(buf);

    // Free the allocated buffer
    free(buf);

    // Return the formatted string
    return log_msg;
}



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
    std::string output = format_string(fmt, args);
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
    std::string output = format_string(fmt, args);
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
    std::string output = format_string(fmt, args);
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
    std::string output = format_string(fmt, args);
    printf("[ERROR]    %s", output.c_str());
    va_end(args);
}
