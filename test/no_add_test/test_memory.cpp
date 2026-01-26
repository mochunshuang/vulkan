#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>

constexpr size_t ARRAY_SIZE = 10000;
constexpr int ITERATIONS = 100;

// 测试栈内存访问
void testStackMemory()
{
    int stackArray[ARRAY_SIZE]; // 栈分配

    // 初始化
    for (size_t i = 0; i < ARRAY_SIZE; ++i)
    {
        stackArray[i] = static_cast<int>(i);
    }

    // 访问测试
    auto start = std::chrono::high_resolution_clock::now();

    volatile int sum = 0; // volatile防止编译器优化
    for (int iter = 0; iter < ITERATIONS; ++iter)
    {
        for (size_t i = 0; i < ARRAY_SIZE; ++i)
        {
            stackArray[i] = stackArray[i] + 1;
            sum += stackArray[i];
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "栈内存访问时间: " << duration << " 微秒" << std::endl;
}

// 测试堆内存访问
void testHeapMemory()
{
    int *heapArray = new int[ARRAY_SIZE]; // 堆分配

    // 初始化
    for (size_t i = 0; i < ARRAY_SIZE; ++i)
    {
        heapArray[i] = static_cast<int>(i);
    }

    // 访问测试
    auto start = std::chrono::high_resolution_clock::now();

    volatile int sum = 0;
    for (int iter = 0; iter < ITERATIONS; ++iter)
    {
        for (size_t i = 0; i < ARRAY_SIZE; ++i)
        {
            heapArray[i] = heapArray[i] + 1;
            sum += heapArray[i];
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "堆内存访问时间: " << duration << " 微秒" << std::endl;

    delete[] heapArray;
}

// 更精确的测试：随机访问模式
void testRandomAccess()
{
    std::vector<size_t> indices(ARRAY_SIZE);
    for (size_t i = 0; i < ARRAY_SIZE; ++i)
    {
        indices[i] = i;
    }

    // 打乱访问顺序
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);

    // 栈内存测试
    {
        int stackArray[ARRAY_SIZE];
        for (size_t i = 0; i < ARRAY_SIZE; ++i)
        {
            stackArray[i] = static_cast<int>(i);
        }

        auto start = std::chrono::high_resolution_clock::now();

        volatile int sum = 0;
        for (int iter = 0; iter < 10; ++iter)
        { // 减少迭代次数，因为随机访问慢
            for (size_t idx : indices)
            {
                stackArray[idx] = stackArray[idx] + 1;
                sum += stackArray[idx];
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "栈内存随机访问时间: " << duration << " 微秒" << std::endl;
    }

    // 堆内存测试
    {
        int *heapArray = new int[ARRAY_SIZE];
        for (size_t i = 0; i < ARRAY_SIZE; ++i)
        {
            heapArray[i] = static_cast<int>(i);
        }

        auto start = std::chrono::high_resolution_clock::now();

        volatile int sum = 0;
        for (int iter = 0; iter < 10; ++iter)
        {
            for (size_t idx : indices)
            {
                heapArray[idx] = heapArray[idx] + 1;
                sum += heapArray[idx];
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::cout << "堆内存随机访问时间: " << duration << " 微秒" << std::endl;

        delete[] heapArray;
    }
}

// 测试缓存友好性（步长访问）
void testCachePerformance()
{
    constexpr size_t SIZE = 10000000;
    constexpr int STEP = 16; // 大步长，测试缓存不友好

    // 堆分配
    int *data = new int[SIZE];
    for (size_t i = 0; i < SIZE; ++i)
    {
        data[i] = 1;
    }

    // 顺序访问（缓存友好）
    auto start1 = std::chrono::high_resolution_clock::now();
    volatile int sum1 = 0;
    for (size_t i = 0; i < SIZE; ++i)
    {
        sum1 += data[i];
    }
    auto end1 = std::chrono::high_resolution_clock::now();

    // 大步长访问（缓存不友好）
    auto start2 = std::chrono::high_resolution_clock::now();
    volatile int sum2 = 0;
    for (size_t i = 0; i < SIZE; i += STEP)
    {
        sum2 += data[i];
    }
    auto end2 = std::chrono::high_resolution_clock::now();

    std::cout << "\n缓存性能测试（堆内存）:" << std::endl;
    std::cout
        << "顺序访问: "
        << std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1).count()
        << " 微秒" << std::endl;
    std::cout
        << "大步长访问: "
        << std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2).count()
        << " 微秒" << std::endl;

    delete[] data;
}

int main()
{
    std::cout << "=== 内存访问性能测试 ===" << std::endl;
    std::cout << "数组大小: " << ARRAY_SIZE << " 元素" << std::endl;
    std::cout << "迭代次数: " << ITERATIONS << " 次\n" << std::endl;

    // 运行测试
    testStackMemory();
    testHeapMemory();

    std::cout << "\n=== 随机访问测试 ===" << std::endl;
    testRandomAccess();

    testCachePerformance();

    return 0;
}