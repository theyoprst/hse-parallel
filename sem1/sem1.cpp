#include <algorithm>
#include <execution>
#include <functional>
#include <future>
#include <iostream>
#include <random>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "profile.h"


template <typename FunctionResult, typename... FunctionArgs>
class Checker {
public:
    using Function = FunctionResult (*)(FunctionArgs...);
    using ResultChecker = std::function<bool(const FunctionResult&)>;
    struct Test {
        ResultChecker result_checker;
        std::tuple<FunctionArgs...> args;
    };

    Checker(Function function) : function_(function) {}

    template <typename... Args>
    void AddTest(ResultChecker result_checker, Args&&... args) {
        tests_.push_back({
            result_checker,
            std::tuple<FunctionArgs...>{std::forward<Args>(args)...},
        });
    }

    void ClearTests() {
        tests_.clear();
    }

    // простой запуск с выводом результата
    void RunSeq() const {
        for (size_t test_index = 0; test_index < tests_.size(); ++test_index) {
            const Test& test = tests_[test_index];
            const bool result = test.result_checker(std::apply(function_, test.args));
            std::cerr << "Test " << test_index << (result ? " OK" : " Fail") << std::endl;
        }
    }

    // запускаем асинхронно, затем выводим результат
    void RunAsyncPrintAfter() const {
        std::vector<std::future<bool>> futures;
        for (const Test& test : tests_) {
            futures.push_back(std::async([&test, function = function_]{
                return test.result_checker(std::apply(function, test.args));
            }));
        }
        for (size_t test_index = 0; test_index < tests_.size(); ++test_index) {
            std::cerr << "Test " << test_index <<
                (futures[test_index].get() ? " OK" : " Fail") << std::endl;
        }
    }

    // запускаем асинхронно, результат выводим сразу
    void RunAsyncPrintEarly() const {
        std::vector<std::future<void>> futures;
        for (size_t test_index = 0; test_index < tests_.size(); ++test_index) {
            const Test& test = tests_[test_index];
            futures.push_back(std::async([&test, test_index, function = function_]{
                const bool result = test.result_checker(std::apply(function, test.args));
                std::cerr << "Test " << test_index << (result ? " OK" : " Fail") << std::endl;
            }));
        }
        for (auto& future : futures) {
            future.get();
        }
    }

    // запускаем асинхронно, неправильно подсчитываем количество
    void RunAsyncCountOksNaive() const {
        std::vector<std::future<void>> futures;
        size_t ok_count = 0;
        for (const Test& test : tests_) {
            futures.push_back(std::async([&test, function = function_, &ok_count]{
                const bool result = test.result_checker(std::apply(function, test.args));
                ok_count += result;  // race condition
            }));
        }
        for (auto& future : futures) {
            future.get();
        }
        std::cerr << ok_count << "/" << tests_.size() << " tests are OK" << std::endl;
    }

    // запускаем асинхронно, неправильно подсчитываем количество с помощью мьютекса
    void RunAsyncCountOksLocalMutex() const {
        std::vector<std::future<void>> futures;
        size_t ok_count = 0;
        for (const Test& test : tests_) {
            futures.push_back(std::async([&test, function = function_, &ok_count]{
                const bool result = test.result_checker(std::apply(function, test.args));
                std::mutex m;
                m.lock();
                ok_count += result;  // race condition
                m.unlock();
            }));
        }
        for (auto& future : futures) {
            future.get();
        }
        std::cerr << ok_count << "/" << tests_.size() << " tests are OK" << std::endl;
    }

    // запускаем асинхронно, подсчитываем количество с помощью мьютекса
    // работает верно, но критическая область слишком большая
    void RunAsyncCountOksWideMutex() const {
        std::vector<std::future<void>> futures;
        size_t ok_count = 0;
        std::mutex counter_mutex;
        for (const Test& test : tests_) {
            futures.push_back(std::async([&test, function = function_, &ok_count, &counter_mutex]{
                std::lock_guard guard(counter_mutex);
                const bool result = test.result_checker(std::apply(function, test.args));
                ok_count += result;
            }));
        }
        for (auto& future : futures) {
            future.get();
        }
        std::cerr << ok_count << "/" << tests_.size() << " tests are OK" << std::endl;
    }

    // запускаем асинхронно, подсчитываем количество с помощью мьютекса
    // работает верно, в критической области только счётчик
    void RunAsyncCountOksRightMutex() const {
        std::vector<std::future<void>> futures;
        size_t ok_count = 0;
        std::mutex counter_mutex;
        for (const Test& test : tests_) {
            futures.push_back(std::async([&test, function = function_, &ok_count, &counter_mutex]{
                const bool result = test.result_checker(std::apply(function, test.args));
                std::lock_guard guard(counter_mutex);
                ok_count += result;
            }));
        }
        for (auto& future : futures) {
            future.get();
        }
        std::cerr << ok_count << "/" << tests_.size() << " tests are OK" << std::endl;
    }

    // запускаем асинхронно, подсчитываем количество с помощью атомарного счётчика
    void RunAsyncCountOksAtomic() const {
        std::vector<std::future<void>> futures;
        std::atomic_int ok_count = 0;
        for (const Test& test : tests_) {
            futures.push_back(std::async([&test, function = function_, &ok_count]{
                const bool result = test.result_checker(std::apply(function, test.args));
                ok_count += result;
            }));
        }
        for (auto& future : futures) {
            future.get();
        }
        std::cerr << ok_count << "/" << tests_.size() << " tests are OK" << std::endl;
    }

    // циклом подсчитываем количество, оказывается гораздо быстрее
    void RunSeqCountOks() const {
        size_t ok_count = 0;
        for (const Test& test : tests_) {
            const bool result = test.result_checker(std::apply(function_, test.args));
            ok_count += result;
        }
        std::cerr << ok_count << "/" << tests_.size() << " tests are OK" << std::endl;
    }

    // подсчитываем количество с помощью transform_reduce, но подследовательно
    void RunCountOksTRSeq() const {
        const size_t ok_count = std::transform_reduce(
            std::execution::seq,
            tests_.begin(), tests_.end(),
            0u,
            std::plus<unsigned>{},
            [function = function_](const Test& test) {
                return test.result_checker(std::apply(function, test.args));
            }
        );
        std::cerr << ok_count << "/" << tests_.size() <<
            " tests are OK" << std::endl;
    }

    // подсчитываем количество с помощью transform_reduce, параллельно
    void RunCountOksTRPar() const {
        const size_t ok_count = std::transform_reduce(
            std::execution::par,
            tests_.begin(), tests_.end(),
            0u,
            std::plus<unsigned>{},
            [function = function_](const Test& test) {
                return test.result_checker(std::apply(function, test.args));
            }
        );
        std::cerr << ok_count << "/" << tests_.size() << " tests are OK" << std::endl;
    }

    void RunAsyncCountOksAtomicThreadPool() const {
        std::vector<std::thread> thread_pool;
        std::atomic_int cur_test = -1;
        std::atomic_int ok_count = 0;
        for (int i = 0; i < std::thread::hardware_concurrency(); ++i) {
            thread_pool.emplace_back(
                [&cur_test, &ok_count, tests = &tests_, function = function_](){
                    while (true) {
                        int next_test = ++cur_test;
                        if (next_test >= tests->size()) {
                            break;
                        }
                        const Test& test = (*tests)[next_test];
                        ok_count += test.result_checker(std::apply(function, test.args));
                    }
                }
            );
        }
        for (std::thread& t : thread_pool) {
            t.join();
        }
        std::cerr << ok_count << "/" << tests_.size() <<
            " tests are OK, #threads = " << thread_pool.size() << std::endl;
    }

private:
    Function function_;
    std::vector<Test> tests_;
};


std::vector<std::string> SplitIntoWords(std::string_view text) {
    std::vector<std::string> words = {""};
    for (const char c : text) {
        if (c == ' ') {
            words.emplace_back();
        } else {
            words.back().push_back(c);
        }
    }
    return words;
}

std::string GenerateQuery(std::mt19937& generator, int max_length, int space_rate) {
    const int length = std::uniform_int_distribution(1, max_length)(generator);
    std::string query(length, ' ');
    for (char& c : query) {
        const int rnd = std::uniform_int_distribution(0, space_rate - 1)(generator);
        if (rnd > 0) {
            c = 'a' + (rnd - 1);
        }
    }
    return query;
}

std::vector<std::string> GenerateQueries(std::mt19937& generator, int query_count, int max_length,
                                         int space_rate) {
    std::vector<std::string> queries;
    queries.reserve(query_count);
    for (int i = 0; i < query_count; ++i) {
        queries.push_back(GenerateQuery(generator, max_length, space_rate));
    }
    return queries;
}

template <typename Checker>
void AddQueriesToCheck(Checker& checker, const std::vector<std::string>& queries) {
    for (const std::string& query : queries) {
        const size_t space_count = std::count(query.begin(), query.end(), ' ');
        checker.AddTest(
            [space_count](const std::vector<std::string>& words) {
                return words.size() == space_count + 1;
            },
            query
        );
    }
}


int main() {
    Checker checker(SplitIntoWords);
    checker.AddTest(
        [](const std::vector<std::string>& words) {
            return words == std::vector<std::string>{"aaa", "aa"};
        },
        "aaa aa"
    );

    std::mt19937 generator;

#define PROFILE(method) { LOG_DURATION(#method); checker.method(); }

    // прогоняем тесты, выводя результат каждого

    const auto long_queries = GenerateQueries(generator, 10, 20'000'000, 4);
    AddQueriesToCheck(checker, long_queries);

    PROFILE(RunSeq);
    PROFILE(RunAsyncPrintAfter);
    PROFILE(RunAsyncPrintEarly);

    checker.ClearTests();
    std::cerr << std::endl;

    // прогоняем тесты, выводя количество успешных

    const auto short_queries = GenerateQueries(generator, 100'000, 10, 4);
    AddQueriesToCheck(checker, short_queries);

    PROFILE(RunAsyncCountOksNaive);
    PROFILE(RunAsyncCountOksLocalMutex);
    PROFILE(RunAsyncCountOksWideMutex);
    PROFILE(RunAsyncCountOksRightMutex);
    PROFILE(RunAsyncCountOksAtomic);
    PROFILE(RunSeqCountOks);

    checker.ClearTests();
    std::cerr << std::endl;

    // больше тестов

    const auto more_short_queries = GenerateQueries(generator, 10'000'000, 10, 4);
    AddQueriesToCheck(checker, more_short_queries);

    PROFILE(RunSeqCountOks);
    PROFILE(RunCountOksTRSeq);
    PROFILE(RunCountOksTRPar);
    PROFILE(RunAsyncCountOksAtomicThreadPool);
}
