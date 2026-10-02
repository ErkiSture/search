#pragma once

#include"clock.h"
#include<chrono>

class SystemClock : public Clock {
public:
	std::chrono::steady_clock::time_point now() const override {
		return std::chrono::steady_clock::now();
	}
};