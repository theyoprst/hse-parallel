#include <future>
#include <thread>
#include <vector>

#include "profile.h"

int NUM_TESTS = std::thread::hardware_concurrency();

void SlowFunction() {
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void Seq() {
	for (int i = 0; i < NUM_TESTS; ++i) {
		SlowFunction();
	}
}

void Async() {
	std::vector<std::future<void>> futures;
	for (int i = 0; i < NUM_TESTS; ++i) {
		futures.push_back(std::async(std::launch::async, SlowFunction));
	}
}

#define PROFILE(function) { LOG_DURATION(#function); function(); }

int main() {
	PROFILE(Seq);
	PROFILE(Async);
}