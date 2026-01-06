#include <iostream>
#include <chrono>

// NOLINTBEGIN

// ==================== 手写vtable版本 ====================
template <typename Func>
class ManualVTable
{
    struct VTable
    {
        void (*invoke)(void *, int &);
    };

    template <typename T>
    static constexpr VTable *get_vtable()
    {
        static VTable vt = {
            [](void *ptr, int &value) { (*static_cast<T *>(ptr))(value); }};
        return &vt;
    }

    VTable *vtable;
    Func func;

  public:
    ManualVTable(Func f) : vtable(get_vtable<Func>()), func(f) {}

    void invoke(int &value)
    {
        vtable->invoke(&func, value);
    }
};

// ==================== C++虚函数版本 ====================
class VirtualBase
{
  public:
    virtual ~VirtualBase() = default;
    virtual void invoke(int &value) = 0;
};

template <typename Func>
class VirtualImpl : public VirtualBase
{
    Func func;

  public:
    VirtualImpl(Func f) : func(f) {}

    void invoke(int &value) override
    {
        func(value);
    }
};

// ==================== 测试函数 ====================
struct TestFunctor
{
    int multiplier;

    void operator()(int &value) const
    {
        value += multiplier;
    }
};

void test_func(int &value)
{
    value += 5;
}

// ==================== 测试代码 ====================
// 测试手写虚函数表
template <typename Func>
void test_manual_performance(ManualVTable<Func> *manual, int iterations, int &result)
{
    for (int i = 0; i < iterations; ++i)
    {
        manual->invoke(result);
    }
}

// 测试C++虚函数
void test_virtual_performance(VirtualBase *base, int iterations, int &result)
{
    for (int i = 0; i < iterations; ++i)
    {
        base->invoke(result);
    }
}

// 测试直接调用
template <typename Func>
void test_direct_performance(Func *func, int iterations, int &result)
{
    for (int i = 0; i < iterations; ++i)
    {
        (*func)(result);
    }
}

// 主测试函数
template <typename Func>
void run_test(const char *name, Func func)
{
    constexpr int iterations = 100000000;
    int manual_result = 0;
    int virtual_result = 0;
    int direct_result = 0;

    // 创建对象 - 在循环外创建防止优化
    ManualVTable<Func> manual(func);
    VirtualImpl<Func> impl(func);
    VirtualBase *base = &impl;

    // 测试手写虚函数表
    auto start1 = std::chrono::high_resolution_clock::now();
    test_manual_performance(&manual, iterations, manual_result);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);

    // 测试C++虚函数
    auto start2 = std::chrono::high_resolution_clock::now();
    test_virtual_performance(base, iterations, virtual_result);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);

    // 测试直接调用
    auto start3 = std::chrono::high_resolution_clock::now();
    test_direct_performance(&func, iterations, direct_result);
    auto end3 = std::chrono::high_resolution_clock::now();
    auto duration3 = std::chrono::duration_cast<std::chrono::milliseconds>(end3 - start3);

    std::cout << "测试: " << name << "\n";
    std::cout << "  手写虚函数表: " << duration1.count() << "ms, 结果: " << manual_result
              << "\n";
    std::cout << "  C++虚函数: " << duration2.count() << "ms, 结果: " << virtual_result
              << "\n";
    std::cout << "  直接调用: " << duration3.count() << "ms, 结果: " << direct_result
              << "\n";

    if (duration1.count() > 0 && duration2.count() > 0)
    {
        double ratio = static_cast<double>(duration2.count()) / duration1.count();
        std::cout << "  性能比 (C++虚函数/手写vtable): " << ratio << " ("
                  << (ratio > 1.0 ? "手写更快" : "C++更快") << ")\n";
    }
    std::cout << std::endl;
}

int main()
{
    std::cout << "性能对比测试 (1亿次调用)\n";
    std::cout << "========================\n\n";

    // 测试1: 函数对象
    TestFunctor functor{2};
    run_test("函数对象 (multiplier=2)", functor);

    // 测试2: 函数指针
    run_test("函数指针", test_func);

    // 测试3: lambda表达式
    auto lambda = [](int &value) {
        value += 3;
    };
    run_test("Lambda表达式", lambda);

    // 测试4: 内联函数对象
    run_test("临时函数对象", TestFunctor{1});

    // 测试5: 复杂lambda捕获
    int capture = 10;
    auto lambda_capture = [capture](int &value) {
        value += capture;
    };
    run_test("带捕获的Lambda", lambda_capture);

    return 0;
}
// NOLINTEND