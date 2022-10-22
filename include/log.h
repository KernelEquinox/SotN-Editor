#ifndef SOTN_EDITOR_LOG_H
#define SOTN_EDITOR_LOG_H

#include <string>
#include "common.h"



// Log level enumeration
enum LOG_LEVEL {
    LOG_NONE = 0,
    LOG_ERROR = 1,
    LOG_WARN = 2,
    LOG_INFO = 3,
    LOG_DEBUG = 4
};



// Class for logging messages to console
class Log {

    public:

        // Enable debug logging
        static uint level;

        static void Debug(const char* fmt, ...);
        static void Info(const char* fmt, ...);
        static void Warn(const char* fmt, ...);
        static void Error(const char* fmt, ...);



    private:

};

#endif //SOTN_EDITOR_LOG_H
