// NOLINTBEGIN
// NOLINTBEGIN
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <random>
#include <memory>

constexpr size_t ITERATIONS = 10'000'000;
constexpr size_t SMALL_ITERATIONS = 5'000'000;

// 使用动态分配，避免栈溢出
std::unique_ptr<int[]> generate_random_inputs(size_t count)
{
    auto inputs = std::make_unique<int[]>(count);
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist{1, 100};

    for (size_t i = 0; i < count; ++i)
    {
        inputs[i] = dist(rng);
    }
    return inputs;
}

// 简化版基准测试
template <typename Func>
double simple_benchmark(const char *name, Func &&func, size_t iterations)
{
    // 预热
    for (size_t i = 0; i < std::min(iterations, 1000ULL); ++i)
    {
        func(i);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < iterations; ++i)
    {
        func(i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double ns_per_op = duration.count() / double(iterations);
    std::cout << name << ": " << ns_per_op << " ns/op\n";
    return ns_per_op;
}

void fixed_atomic_test()
{
    std::cout << "\n=== FIXED ATOMIC PERFORMANCE TEST ===\n";

    // 预生成输入数据（堆分配）
    auto inputs = generate_random_inputs(ITERATIONS);

    // 测试1：普通赋值
    double normal_time = simple_benchmark(
        "Normal assignment",
        [&](size_t i) {
            int value = inputs[i];
            int result = value * 2 + 1;
            volatile int sink = result;
        },
        ITERATIONS);

    // 测试2：原子存储 (relaxed)
    double atomic_relaxed_time = simple_benchmark(
        "Atomic store (relaxed)",
        [&](size_t i) {
            thread_local std::atomic<int> atomic_val{0}; // 使用thread_local
            atomic_val.store(inputs[i], std::memory_order_relaxed);
            volatile int sink = atomic_val.load();
        },
        ITERATIONS);

    // 测试3：原子存储 (seq_cst)
    double atomic_seq_cst_time = simple_benchmark(
        "Atomic store (seq_cst)",
        [&](size_t i) {
            thread_local std::atomic<int> atomic_val{0};
            atomic_val.store(inputs[i], std::memory_order_seq_cst);
            volatile int sink = atomic_val.load();
        },
        ITERATIONS);

    // 测试4：原子fetch_add
    {
        static std::atomic<int> global_counter{0};
        double atomic_fetch_add_time = simple_benchmark(
            "Atomic fetch_add",
            [&](size_t i) {
                global_counter.fetch_add(inputs[i], std::memory_order_relaxed);
            },
            ITERATIONS);

        std::cout << "Fetch_add final value: " << global_counter.load() << "\n";
    }

    std::cout << "\n=== RELATIVE PERFORMANCE ===\n";
    std::cout << "Atomic relaxed vs Normal: " << atomic_relaxed_time / normal_time
              << "x\n";
    std::cout << "Atomic seq_cst vs Normal: " << atomic_seq_cst_time / normal_time
              << "x\n";
}

// 安全的计数器测试
void safe_counter_test()
{
    constexpr int THREAD_COUNT = 4;

    std::cout << "\n=== SAFE COUNTER TEST ===\n";

    // 单线程测试
    {
        std::atomic<int> counter{0};
        auto inputs = generate_random_inputs(SMALL_ITERATIONS);

        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < SMALL_ITERATIONS; ++i)
        {
            counter.fetch_add(inputs[i], std::memory_order_relaxed);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "Single-threaded atomic: "
                  << duration.count() / double(SMALL_ITERATIONS) << " ns/op\n";
        std::cout << "Final: " << counter.load() << "\n";
    }

    // 多线程测试（简化）
    {
        std::atomic<int> shared_counter{0};
        std::vector<std::thread> threads;
        constexpr size_t PER_THREAD = SMALL_ITERATIONS / THREAD_COUNT;

        // 每个线程自己的输入数据
        std::vector<std::unique_ptr<int[]>> thread_inputs;
        for (int t = 0; t < THREAD_COUNT; ++t)
        {
            thread_inputs.push_back(generate_random_inputs(PER_THREAD));
        }

        auto start = std::chrono::high_resolution_clock::now();

        for (int t = 0; t < THREAD_COUNT; ++t)
        {
            threads.emplace_back([&, t]() {
                auto &my_inputs = thread_inputs[t];
                for (size_t i = 0; i < PER_THREAD; ++i)
                {
                    shared_counter.fetch_add(my_inputs[i], std::memory_order_relaxed);
                }
            });
        }

        for (auto &t : threads)
            t.join();

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "\nMulti-threaded (" << THREAD_COUNT
                  << " threads) atomic: " << duration.count() / double(SMALL_ITERATIONS)
                  << " ns/op\n";
        std::cout << "Final: " << shared_counter.load() << "\n";
    }
}

// 最基本的测试：证明观点
void minimal_test()
{
    std::cout << "\n=== MINIMAL TEST (证明观点) ===\n";

    constexpr size_t TEST_ITERATIONS = 1'000'000;
    auto inputs = generate_random_inputs(TEST_ITERATIONS);

    // 只用最简单的对比
    {
        int sum = 0;
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < TEST_ITERATIONS; ++i)
        {
            sum += inputs[i];
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto normal_time =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "Normal sum: " << normal_time.count() / double(TEST_ITERATIONS)
                  << " ns/op\n";
        volatile int dummy = sum;
    }

    {
        std::atomic<int> atomic_sum{0};
        auto start = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < TEST_ITERATIONS; ++i)
        {
            atomic_sum.fetch_add(inputs[i], std::memory_order_relaxed);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto atomic_time =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "Atomic sum: " << atomic_time.count() / double(TEST_ITERATIONS)
                  << " ns/op\n";
    }
}

int main()
{
    std::cout << "SAFE ATOMIC PERFORMANCE TEST\n";
    std::cout << "=============================\n";

    try
    {
        fixed_atomic_test();
        safe_counter_test();
        minimal_test();

        std::cout << "\n=== 总结 ===\n";
        std::cout << "1. 预生成数据消除随机生成开销\n";
        std::cout << "2. 使用动态分配避免栈溢出\n";
        std::cout << "3. 简化测试确保稳定性\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    catch (...)
    {
        std::cerr << "未知错误" << std::endl;
        return 1;
    }

    return 0;
}
/*
Normal assignment: 0.699 ns/op
Atomic store (relaxed): 0.729 ns/op   # 仅慢4%
Atomic store (seq_cst): 2.845 ns/op   # 慢306%
//NOTE: 2 倍的开销
Atomic fetch_add: 2.497 ns/op         # 慢257%
 // NOTE: 原子操作：~0.002ms (2ns) - UI事件中占比极小
*/
// NOLINTEND