#include <chrono>
#include "utils/clock.h"

class FakeClock : public Clock {
public:
    FakeClock()
        : time(std::chrono::steady_clock::time_point{})
    {
    }

    std::chrono::steady_clock::time_point now() const override {
        return time;
    }

    void advance(std::chrono::milliseconds amount) {
        time += amount;
    }
private:
    std::chrono::steady_clock::time_point time;
};