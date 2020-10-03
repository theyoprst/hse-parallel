#include <array>
#include <chrono>
#include <future>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "profile.h"

using namespace std;

vector<int> GenerateNumbers(mt19937& generator, int count, int max_value) {
    vector<int> numbers(count);
    for (int& x : numbers) {
        x = uniform_int_distribution(0, max_value)(generator);
    }
    sort(numbers.begin(), numbers.end());
    return numbers;
}

bool CheckNumber(int x) {
    // Emulate very expensive check.
    this_thread::sleep_for(100ms);
    return x > 100'000'000;
}

int FindSimple(const vector<int>& numbers) {
    int left = -1;
    int right = numbers.size();
    while (left + 1 < right) {
        const int med = (left + right) / 2;
        if (CheckNumber(numbers[med])) {
            right = med;
        } else {
            left = med;
        }
    }
    return right;
}

int Find3PartsSeq(const vector<int>& numbers) {
    int left = -1;
    int right = numbers.size();
    while (left + 1 < right) {
        const int dist = max(1, (right - left) / 3);
        const int med1 = left + dist;
        const int med2 = right - dist;
        const bool res1 = CheckNumber(numbers[med1]);
        const bool res2 = CheckNumber(numbers[med2]);
        if (res1) {
            right = med1;
        } else if (!res2) {
            left = med2;
        } else {
            left = med1;
            right = med2;
        }
    }
    return right;
}

int Find3PartsPar(const vector<int>& numbers) {
    int left = -1;
    int right = numbers.size();
    while (left + 1 < right) {
        const int dist = max(1, (right - left) / 3);
        const int med1 = left + dist;
        const int med2 = right - dist;

        future<bool> future1 = async(CheckNumber, numbers[med1]);
        const bool res2 = CheckNumber(numbers[med2]);
        const bool res1 = future1.get();

        if (res1) {
            right = med1;
        } else if (!res2) {
            left = med2;
        } else {
            left = med1;
            right = med2;
        }
    }
    return right;
}

int Find4PartsPar(const vector<int>& numbers) {
    int left = -1;
    int right = numbers.size();
    while (left + 1 < right) {
        const int dist = max(1, (right - left) / 4);
        const int med1 = left + dist;
        const int med2 = med1 + dist;
        const int med3 = right - dist;

        future<bool> future1 = async(CheckNumber, numbers[med1]);
        future<bool> future2 = async(CheckNumber, numbers[med2]);
        const bool res3 = CheckNumber(numbers[med3]);
        const bool res1 = future1.get();
        const bool res2 = future2.get();

        if (res1) {
            right = med1;
        } else if (!res3) {
            left = med3;
        } else if (res2) {
            left = med1;
            right = med2;
        } else {
            left = med2;
            right = med3;
        }
    }
    return right;
}

template <int P>
int FindNBoundsPar(const vector<int>& numbers) {
    int left = -1;
    int right = numbers.size();
    while (left + 1 < right) {
        const int dist = max(1, (right - left) / P);
        array<int, P + 1> bounds;
        array<future<bool>, P + 1> futures;
        for (int i = 0; i < P - 1; ++i) {
            bounds[i] = left + dist * i;
        }
        bounds[P] = right;
        bounds[P - 1] = right - dist;

        for (int i = 1; i < P; ++i) {
            futures[i] = async(CheckNumber, numbers[bounds[i]]);
        }

        for (int i = 1; i <= P; ++i) {
            if (i == P || futures[i].get()) {
                left = bounds[i - 1];
                right = bounds[i];
                break;
            }
        }
    }
    return right;
}

#define TEST(f) { LOG_DURATION(#f); cout << f(numbers) << endl; }

int main() {
    mt19937 generator;
    const auto numbers = GenerateNumbers(generator, 1'000'000, 1'000'000'000);
    TEST(FindSimple);
    TEST(Find3PartsSeq);
    TEST(Find3PartsPar);
    TEST(Find4PartsPar);
    TEST(FindNBoundsPar<2>);
    TEST(FindNBoundsPar<3>);
    TEST(FindNBoundsPar<4>);
    TEST(FindNBoundsPar<5>);
    TEST(FindNBoundsPar<6>);
    TEST(FindNBoundsPar<7>);
    TEST(FindNBoundsPar<8>);
    TEST(FindNBoundsPar<9>);
    TEST(FindNBoundsPar<10>);
    TEST(FindNBoundsPar<11>);
    TEST(FindNBoundsPar<12>);
    return 0;
}
