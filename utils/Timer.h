
#ifndef TIMER_H
#define TIMER_H

#include <chrono>

class Timer {
    const std::chrono::steady_clock::time_point start;
public:
    Timer() : start(std::chrono::steady_clock::now()) { }
    inline std::chrono::nanoseconds time() {
        return std::chrono::steady_clock::now()-start;
    }
    inline bool timeout(int ms) {
        return time() >= std::chrono::milliseconds(ms);
    }
};

#endif
