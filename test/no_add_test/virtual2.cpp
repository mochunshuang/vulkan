#include <iostream>
#include <chrono>

#include <memory>

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

    void operator()(int &value)
    {
        value += multiplier;
    }
};

void test_func(int &value)
{
    value += 5;
}

// ==================== 优化的测试代码 ====================
// 全局变量防止优化
int g_dummy = 0;

// 创建虚函数对象 - 使用工厂模式防止编译器知道具体类型
VirtualBase *create_virtual_obj(int type)
{
    static TestFunctor f1{1};
    static TestFunctor f2{2};

    if (type % 2 == 0)
    {
        return new VirtualImpl<TestFunctor>(f1);
    }
    else
    {
        return new VirtualImpl<TestFunctor>(f2);
    }
}

// 创建手写vtable对象
ManualVTable<TestFunctor> *create_manual_obj(int type)
{
    static TestFunctor f1{1};
    static TestFunctor f2{2};

    if (type % 2 == 0)
    {
        return new ManualVTable<TestFunctor>(f1);
    }
    else
    {
        return new ManualVTable<TestFunctor>(f2);
    }
}

// 简单的公平测试
void run_simple_fair_test()
{
    constexpr int iterations = 10000000; // 1千万次

    std::cout << "公平对比测试 (1000万次调用)\n";
    std::cout << "===========================\n\n";

    // 使用相同的函数对象
    TestFunctor func{2};

    // 测试1: 手写虚函数表
    {
        // 使用动态分配防止优化
        std::unique_ptr<ManualVTable<TestFunctor>> manual(create_manual_obj(g_dummy));
        int result = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            manual->invoke(result);
            // 防止循环优化
            if (i % 10000 == 0)
            {
                g_dummy ^= result;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "手写虚函数表: " << duration.count() << "ms, 结果: " << result
                  << "\n";
    }

    // 测试2: C++虚函数 - 使用动态分配防止优化
    {
        std::unique_ptr<VirtualBase> base(create_virtual_obj(g_dummy));
        int result = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            base->invoke(result);
            // 防止循环优化
            if (i % 10000 == 0)
            {
                g_dummy ^= result;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "C++虚函数: " << duration.count() << "ms, 结果: " << result << "\n";
    }

    // 测试3: 直接调用 - 使用函数指针防止内联
    {
        int result = 0;
        auto start = std::chrono::high_resolution_clock::now();

        // 使用函数指针防止编译器内联优化
        void (*func_ptr)(TestFunctor &, int &) = [](TestFunctor &f, int &v) {
            f(v);
        };

        for (int i = 0; i < iterations; ++i)
        {
            func_ptr(func, result);
            // 防止循环优化
            if (i % 10000 == 0)
            {
                g_dummy ^= result;
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "直接调用: " << duration.count() << "ms, 结果: " << result << "\n";
    }

    // 测试4: 更公平的测试 - 都通过指针调用
    {
        std::cout << "\n更公平的测试 - 都通过指针调用:\n";

        // 手写vtable指针
        std::unique_ptr<ManualVTable<TestFunctor>> manual(create_manual_obj(g_dummy));
        ManualVTable<TestFunctor> *manual_ptr = manual.get();

        // C++虚函数指针
        std::unique_ptr<VirtualBase> base(create_virtual_obj(g_dummy));
        VirtualBase *virtual_ptr = base.get();

        // 直接调用的函数指针
        void (*direct_ptr)(TestFunctor &, int &) = [](TestFunctor &f, int &v) {
            f(v);
        };

        int manual_result = 0;
        int virtual_result = 0;
        int direct_result = 0;

        auto start1 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i)
        {
            manual_ptr->invoke(manual_result);
        }
        auto end1 = std::chrono::high_resolution_clock::now();

        auto start2 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i)
        {
            virtual_ptr->invoke(virtual_result);
        }
        auto end2 = std::chrono::high_resolution_clock::now();

        auto start3 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i)
        {
            direct_ptr(func, direct_result);
        }
        auto end3 = std::chrono::high_resolution_clock::now();

        auto duration1 =
            std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1);
        auto duration2 =
            std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2);
        auto duration3 =
            std::chrono::duration_cast<std::chrono::milliseconds>(end3 - start3);

        std::cout << "手写vtable指针调用: " << duration1.count() << "ms\n";
        std::cout << "C++虚函数指针调用: " << duration2.count() << "ms\n";
        std::cout << "函数指针调用: " << duration3.count() << "ms\n";

        if (duration1.count() > 0 && duration2.count() > 0)
        {
            double ratio = static_cast<double>(duration2.count()) / duration1.count();
            std::cout << "性能比 (C++虚函数/手写vtable): " << ratio << "\n";
        }
    }
}

int main()
{
    // 初始化随机种子
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    g_dummy = std::rand();

    // 运行测试
    run_simple_fair_test();

    return 0;
}
// NOLINTEND