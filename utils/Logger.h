
#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#define printf(...) printf(__VA_ARGS__); fflush(stdout)

class Logger {
    Logger() = default;
    ~Logger() = default;

    static bool enabled;
public:
    
    template <typename... T>
    static inline void log(T... args) { if(Logger::enabled) printf(args...); }

    static inline void enable(bool e) { Logger::enabled = e; }
};

#endif
