#include <algorithm>
#include <atomic>
#include <cstdint>
#include <execution>
#include <functional>
#include <iterator>
#include <mutex>
#include <random>
#include <string_view>

#include "profile.h"

using namespace std;


class Graph {
public:
    Graph(int vertex_count)
        : adjacency_lists_(vertex_count),
          vertex_weights_(vertex_count) {}

    void AddEdge(int vertex_from, int vertex_to) {
        adjacency_lists_[vertex_from].push_back(vertex_to);
    }

    int GetVertexCount() const {
        return adjacency_lists_.size();
    }

    const vector<int>& GetAdjacentVertices(int vertex) const {
        return adjacency_lists_[vertex];
    }

    int operator[](int vertex) const {
        return vertex_weights_[vertex];
    }

    int& operator[](int vertex) {
        return vertex_weights_[vertex];
    }

private:
    vector<vector<int>> adjacency_lists_;
    vector<int> vertex_weights_;
};

Graph GenerateTree(mt19937& generator, int vertex_count, int max_weight) {
    Graph graph(vertex_count);
    for (int vertex = 0; vertex < vertex_count; ++vertex) {
        graph[vertex] = uniform_int_distribution(0, max_weight)(generator);
        if (vertex > 0) {
            const int parent = uniform_int_distribution(0, vertex - 1)(generator);
            graph.AddEdge(parent, vertex);
        }
    }
    return graph;
}

uint64_t ComputeSumSimple(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    vector<int> vertices_to_process = { 0 };
    vector<int> next_vertices;
    while (!vertices_to_process.empty()) {
        ++depth;
        for (const int vertex_from : vertices_to_process) {
            sum += static_cast<uint64_t>(graph[vertex_from]) * depth;
            for (const int vertex_to : graph.GetAdjacentVertices(vertex_from)) {
                next_vertices.push_back(vertex_to);
            }
        }
        vertices_to_process.swap(next_vertices);
        next_vertices.clear();
    }
    return sum;
}

uint64_t ComputeSumFail(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    vector<int> vertices_to_process = { 0 };
    vector<int> next_vertices;
    while (!vertices_to_process.empty()) {
        ++depth;

        sum = transform_reduce(
            execution::par,
            vertices_to_process.begin(), vertices_to_process.end(),
            sum,
            plus<>{},
            [&graph, &next_vertices, depth](int vertex) {
                const auto& children = graph.GetAdjacentVertices(vertex);
                copy(
                    children.begin(), children.end(),
                    back_inserter(next_vertices)
                );
                return static_cast<uint64_t>(graph[vertex]) * depth;
            }
        );

        vertices_to_process.swap(next_vertices);
        next_vertices.clear();
    }
    return sum;
}

uint64_t ComputeSumPoolSimple(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    const int vertex_count = graph.GetVertexCount();
    vector<int> pool(vertex_count, 0);
    for (int from = 0, to = 1, next_to = 1; from < vertex_count; from = to, to = next_to) {
        ++depth;
        for (int i = from; i < to; ++i) {
            const int vertex = pool[i];
            sum += static_cast<uint64_t>(graph[vertex]) * depth;
            for (const int child : graph.GetAdjacentVertices(vertex)) {
                pool[next_to++] = child;
            }
        }
    }
    return sum;
}

uint64_t ComputeSumPoolSeq(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    const int vertex_count = graph.GetVertexCount();
    vector<int> pool(vertex_count, 0);
    vector<int> states(vertex_count);
    for (int from = 0, to = 1, next_to = 1; from < vertex_count; from = to, to = next_to) {
        ++depth;

        transform_exclusive_scan(
            execution::seq,
            pool.begin() + from, pool.begin() + to,
            states.begin() + from,
            next_to,
            plus<>{},
            [&graph](int vertex) -> int {
                return graph.GetAdjacentVertices(vertex).size();
            }
        );
        sum = transform_reduce(
            execution::seq,
            pool.begin() + from, pool.begin() + to,
            states.begin() + from,
            sum,
            plus<>{},
            [&graph, &pool, depth, next_to](int vertex, int local_to) {
                const auto& children = graph.GetAdjacentVertices(vertex);
                copy(
                    children.begin(), children.end(),
                    pool.begin() + local_to
                );
                return static_cast<uint64_t>(graph[vertex]) * depth;
            }
        );
        next_to = states[to - 1] + graph.GetAdjacentVertices(pool[to - 1]).size();
    }
    return sum;
}

uint64_t ComputeSumPoolPar(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    const int vertex_count = graph.GetVertexCount();
    vector<int> pool(vertex_count, 0);
    vector<int> states(vertex_count);
    for (int from = 0, to = 1, next_to = 1; from < vertex_count; from = to, to = next_to) {
        ++depth;

        transform_exclusive_scan(
            execution::par,
            pool.begin() + from, pool.begin() + to,
            states.begin() + from,
            next_to,
            plus<>{},
            [&graph](int vertex) -> int {
                return graph.GetAdjacentVertices(vertex).size();
            }
        );
        sum = transform_reduce(
            execution::par,
            pool.begin() + from, pool.begin() + to,
            states.begin() + from,
            sum,
            plus<>{},
            [&graph, &pool, depth, next_to](int vertex, int local_to) {
                const auto& children = graph.GetAdjacentVertices(vertex);
                copy(
                    children.begin(), children.end(),
                    pool.begin() + local_to
                );
                return static_cast<uint64_t>(graph[vertex]) * depth;
            }
        );
        next_to = states[to - 1] + graph.GetAdjacentVertices(pool[to - 1]).size();
    }
    return sum;
}

uint64_t ComputeSumSeq(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    vector<int> vertices_to_process = { 0 };
    vector<int> next_vertices;

    const int vertex_count = graph.GetVertexCount();
    vector<int> states(vertex_count);

    while (!vertices_to_process.empty()) {
        ++depth;

        // для каждой вершины текущего вектора вычисляем начало диапазона
        // для вставки её детей
        transform_exclusive_scan(
            execution::seq,
            vertices_to_process.begin(), vertices_to_process.end(),
            states.begin(),
            0,
            plus<>{},
            [&graph](int vertex) -> int {
                return graph.GetAdjacentVertices(vertex).size();
            }
        );

        next_vertices.resize(
            states[vertices_to_process.size() - 1]
            + graph.GetAdjacentVertices(vertices_to_process.back()).size());

        // распараллеливаем, не конфликтуя за запись в новый вектор
        sum = transform_reduce(
            execution::seq,
            vertices_to_process.begin(), vertices_to_process.end(),
            states.begin(),
            sum,
            plus<>{},
            [&graph, &next_vertices, depth](int vertex, int local_to) {
                const auto& children = graph.GetAdjacentVertices(vertex);
                copy(
                    children.begin(), children.end(),
                    next_vertices.begin() + local_to
                );
                return static_cast<uint64_t>(graph[vertex]) * depth;
            }
        );

        vertices_to_process.swap(next_vertices);
        next_vertices.clear();
    }
    return sum;
}

uint64_t ComputeSumPar(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    vector<int> vertices_to_process = { 0 };
    vector<int> next_vertices;

    const int vertex_count = graph.GetVertexCount();
    vector<int> states(vertex_count);

    while (!vertices_to_process.empty()) {
        ++depth;

        transform_exclusive_scan(
            execution::par,
            vertices_to_process.begin(), vertices_to_process.end(),
            states.begin(),
            0,
            plus<>{},
            [&graph](int vertex) -> int {
                return graph.GetAdjacentVertices(vertex).size();
            }
        );

        next_vertices.resize(
            states[vertices_to_process.size() - 1]
            + graph.GetAdjacentVertices(vertices_to_process.back()).size());

        sum = transform_reduce(
            execution::par,
            vertices_to_process.begin(), vertices_to_process.end(),
            states.begin(),
            sum,
            plus<>{},
            [&graph, &next_vertices, depth](int vertex, int local_to) {
                const auto& children = graph.GetAdjacentVertices(vertex);
                copy(
                    children.begin(), children.end(),
                    next_vertices.begin() + local_to
                );
                return static_cast<uint64_t>(graph[vertex]) * depth;
            }
        );

        vertices_to_process.swap(next_vertices);
        next_vertices.clear();
    }
    return sum;
}

uint64_t ComputeSumMutex(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    vector<int> vertices_to_process = { 0 };
    vector<int> next_vertices;
    next_vertices.reserve(graph.GetVertexCount());

    mutex m;

    while (!vertices_to_process.empty()) {
        ++depth;

        sum = transform_reduce(
            execution::par,
            vertices_to_process.begin(), vertices_to_process.end(),
            sum,
            plus<>{},
            [&graph, &next_vertices, depth, &m](int vertex) {
                for (const int child : graph.GetAdjacentVertices(vertex)) {
                    lock_guard guard(m);
                    next_vertices.push_back(child);
                }
                return static_cast<uint64_t>(graph[vertex]) * depth;
            }
        );

        vertices_to_process.swap(next_vertices);
        next_vertices.clear();
    }
    return sum;
}

uint64_t ComputeSumSafeVectorRace(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    vector<int> vertices_to_process = { 0 };
    vector<int> next_vertices;

    const int vertex_count = graph.GetVertexCount();
    next_vertices.reserve(vertex_count);

    while (!vertices_to_process.empty()) {
        ++depth;

        next_vertices.resize(vertex_count);
        int place = 0;

        sum = transform_reduce(
            execution::par,
            vertices_to_process.begin(), vertices_to_process.end(),
            sum,
            plus<>{},
            [&graph, &next_vertices, depth, &place](int vertex) {
                for (const int child : graph.GetAdjacentVertices(vertex)) {
                    next_vertices[place++] = child;
                }
                return static_cast<uint64_t>(graph[vertex]) * depth;
            }
        );

        next_vertices.resize(place);

        vertices_to_process.swap(next_vertices);
        next_vertices.clear();
    }
    return sum;
}

uint64_t ComputeSumSafeVectorAtomic(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    vector<int> vertices_to_process = { 0 };
    vector<int> next_vertices;

    const int vertex_count = graph.GetVertexCount();
    next_vertices.reserve(vertex_count);

    while (!vertices_to_process.empty()) {
        ++depth;

        next_vertices.resize(vertex_count);
        atomic_int place = 0;

        sum = transform_reduce(
            execution::par,
            vertices_to_process.begin(), vertices_to_process.end(),
            sum,
            plus<>{},
            [&graph, &next_vertices, depth, &place](int vertex) {
                for (const int child : graph.GetAdjacentVertices(vertex)) {
                    next_vertices[place++] = child;
                }
                return static_cast<uint64_t>(graph[vertex]) * depth;
            }
        );

        next_vertices.resize(place);

        vertices_to_process.swap(next_vertices);
        next_vertices.clear();
    }
    return sum;
}

uint64_t ComputeSumParInner(const Graph& graph) {
    uint64_t sum = 0;
    int depth = 0;
    vector<int> vertices_to_process = { 0 };
    vector<int> next_vertices;
    while (!vertices_to_process.empty()) {
        ++depth;
        next_vertices.resize(graph.GetVertexCount());
        auto next_end = next_vertices.begin();
        for (const int vertex_from : vertices_to_process) {
            sum += static_cast<uint64_t>(graph[vertex_from]) * depth;
            const auto& children = graph.GetAdjacentVertices(vertex_from);
            next_end = copy(execution::par, children.begin(), children.end(), next_end);
        }
        next_vertices.erase(next_end, next_vertices.end());
        vertices_to_process.swap(next_vertices);
        next_vertices.clear();
    }
    return sum;
}

template<typename ComputeSum>
void Test(ComputeSum compute_sum, string_view label, const Graph& graph) {
    uint64_t sum;
    {
        LOG_DURATION(label);
        sum = compute_sum(graph);
    }
    cout << sum << endl;
}

#define TEST(compute_sum) Test([](const Graph& graph) { return compute_sum(graph); }, #compute_sum, graph)


int main() {
    mt19937 generator(12345);
    const Graph graph = GenerateTree(generator, 10'000'000, 1'000);

    // Обычный BFS
    TEST(ComputeSumSimple);

    // Параллелим внешний цикл, не думая: гонка при вставке в вектор
    // TEST(ComputeSumFail);

    // Добавляем мьютекс
    TEST(ComputeSumMutex);

    // Можно не конфликтовать за вставку в вектор, если для каждой вершины текущего вектора знать,
    // куда вставлять её детей
    TEST(ComputeSumSeq);
    TEST(ComputeSumPar);

    // То же самое, но с общим пулом
    // TEST(ComputeSumPoolSimple);
    // TEST(ComputeSumPoolSeq);
    // TEST(ComputeSumPoolPar);

    // Возвращаемся к исходной идее вставки в вектор:
    // вместо push_back записываем в готовую память
    TEST(ComputeSumSafeVectorRace);
    TEST(ComputeSumSafeVectorAtomic);  // спасает atomic-счётчик

    // внутренний цикл не ускоряется
    // TEST(ComputeSumParInner);
}
