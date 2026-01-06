#include <atomic>
#include <chrono>
#include <iostream>
#include <random>

constexpr size_t ITERATIONS = 10'000'000;

// NOLINTBEGIN
// 使用不可预测的值防止优化
int get_random_input()
{
    static std::mt19937 rng{std::random_device{}()};
    static std::uniform_int_distribution<int> dist{1, 100};
    return dist(rng);
}

// 测试普通赋值
long long test_normal_assignment()
{
    auto start = std::chrono::high_resolution_clock::now();

    [[maybe_unused]] int result = 0;
    for (size_t i = 0; i < ITERATIONS; ++i)
    {
        int input = get_random_input(); // 不可预测，防止优化
        result = input;                 // 普通赋值
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// 测试原子存储（relaxed）
long long test_atomic_store_relaxed()
{
    std::atomic<int> atomic_value{0};
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < ITERATIONS; ++i)
    {
        int input = get_random_input();
        atomic_value.store(input, std::memory_order_relaxed);
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// 测试原子存储（seq_cst）
long long test_atomic_store_seq_cst()
{
    std::atomic<int> atomic_value{0};
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < ITERATIONS; ++i)
    {
        int input = get_random_input();
        atomic_value.store(input, std::memory_order_seq_cst);
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

// 测试原子fetch_add
long long test_atomic_fetch_add()
{
    std::atomic<int> atomic_value{0};
    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < ITERATIONS; ++i)
    {
        int input = get_random_input();
        atomic_value.fetch_add(input, std::memory_order_relaxed);
    }

    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
}

int main()
{
    std::cout << "Atomic Operations Performance Test (with optimization)\n";
    std::cout << "Iterations: " << ITERATIONS << "\n";
    std::cout << "=======================================================\n\n";

    // 运行多次取平均
    constexpr int RUNS = 10;
    long long total_normal = 0;
    long long total_atomic_relaxed = 0;
    long long total_atomic_seq_cst = 0;
    long long total_atomic_fetch_add = 0;

    for (int run = 0; run < RUNS; ++run)
    {
        total_normal += test_normal_assignment();
        total_atomic_relaxed += test_atomic_store_relaxed();
        total_atomic_seq_cst += test_atomic_store_seq_cst();
        total_atomic_fetch_add += test_atomic_fetch_add();
    }

    // 计算平均时间
    double avg_normal = total_normal / (double)RUNS;
    double avg_atomic_relaxed = total_atomic_relaxed / (double)RUNS;
    double avg_atomic_seq_cst = total_atomic_seq_cst / (double)RUNS;
    double avg_atomic_fetch_add = total_atomic_fetch_add / (double)RUNS;

    // 计算每操作纳秒数
    double ns_per_op_normal = avg_normal / ITERATIONS;
    double ns_per_op_atomic_relaxed = avg_atomic_relaxed / ITERATIONS;
    double ns_per_op_atomic_seq_cst = avg_atomic_seq_cst / ITERATIONS;
    double ns_per_op_atomic_fetch_add = avg_atomic_fetch_add / ITERATIONS;

    // 输出结果
    std::cout << std::fixed;
    std::cout.precision(2);

    std::cout << "PER OPERATION (nanoseconds):\n";
    std::cout << "Normal assignment:      " << ns_per_op_normal << " ns\n";
    std::cout << "Atomic store (relaxed): " << ns_per_op_atomic_relaxed << " ns";
    std::cout << " (" << (ns_per_op_atomic_relaxed / ns_per_op_normal) << "x slower)\n";

    std::cout << "Atomic store (seq_cst): " << ns_per_op_atomic_seq_cst << " ns";
    std::cout << " (" << (ns_per_op_atomic_seq_cst / ns_per_op_normal) << "x slower)\n";

    std::cout << "Atomic fetch_add:       " << ns_per_op_atomic_fetch_add << " ns";
    std::cout << " (" << (ns_per_op_atomic_fetch_add / ns_per_op_normal) << "x slower)\n";

    std::cout << "\nTOTAL TIME (microseconds):\n";
    std::cout << "Normal assignment:      " << avg_normal / 1000.0 << " μs\n";
    std::cout << "Atomic store (relaxed): " << avg_atomic_relaxed / 1000.0 << " μs\n";
    std::cout << "Atomic store (seq_cst): " << avg_atomic_seq_cst / 1000.0 << " μs\n";
    std::cout << "Atomic fetch_add:       " << avg_atomic_fetch_add / 1000.0 << " μs\n";

    return 0;
}
// NOLINTEND