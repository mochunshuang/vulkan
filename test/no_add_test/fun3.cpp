#include <bit>
#include <cassert>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <chrono>
#include <vector>
#include <array>
#include <iomanip>
#include <cstring>

// ================================
// 修复版：fast_func 优化版本
// ================================

template <typename Signature>
class fast_func;

template <typename R, typename... Args>
class fast_func<R(Args...)>
{
  private:
    // 优化存储：16字节小对象缓冲区
    static constexpr size_t INLINE_SIZE = 16;

    // 存储类型
    union storage_t {
        alignas(8) std::byte inline_data[INLINE_SIZE];
        void *heap_ptr;

        storage_t() noexcept : inline_data{} {}
        ~storage_t() noexcept {}
    };

    // 调用信息
    struct call_info
    {
        void *obj;
        R (*invoke)(void *, Args...);
        void (*cleanup)(void *, bool is_inline) noexcept;
        bool is_inline;
    };

  public:
    // 默认构造
    fast_func() noexcept : info_{nullptr, nullptr, nullptr, false} {}

    // 构造函数 - 针对各种可调用对象
    template <typename F>
    fast_func(F &&f)
    {
        using T = std::decay_t<F>;

        // 判断是否适合内联存储
        constexpr bool fits_inline =
            sizeof(T) <= INLINE_SIZE && alignof(T) <= alignof(std::max_align_t);

        if constexpr (fits_inline)
        {
            // 内联存储
            new (&storage_.inline_data) T(std::forward<F>(f));
            info_.obj = &storage_.inline_data;
            info_.is_inline = true;

            // 设置调用函数
            info_.invoke = [](void *obj, Args... args) -> R {
                return (*static_cast<T *>(obj))(std::forward<Args>(args)...);
            };

            // 设置清理函数 - 内联版本
            info_.cleanup = [](void *obj, bool is_inline) noexcept {
                if (is_inline)
                {
                    static_cast<T *>(obj)->~T();
                }
            };
        }
        else
        {
            // 堆存储
            T *ptr = new T(std::forward<F>(f));
            storage_.heap_ptr = ptr;
            info_.obj = ptr;
            info_.is_inline = false;

            // 设置调用函数
            info_.invoke = [](void *obj, Args... args) -> R {
                return (*static_cast<T *>(obj))(std::forward<Args>(args)...);
            };

            // 设置清理函数 - 堆版本
            info_.cleanup = [](void *obj, bool is_inline) noexcept {
                if (!is_inline)
                {
                    delete static_cast<T *>(obj);
                }
            };
        }
    }

    // 移动构造函数
    fast_func(fast_func &&other) noexcept : info_(other.info_), storage_(other.storage_)
    {
        other.info_ = {nullptr, nullptr, nullptr, false};
        if (info_.is_inline)
        {
            // 内联存储：移动后需要更新指针
            info_.obj = &storage_.inline_data;
        }
    }

    // 析构函数
    ~fast_func()
    {
        if (info_.cleanup)
        {
            info_.cleanup(info_.obj, info_.is_inline);
        }
    }

    // 调用操作符
    R operator()(Args... args) const
    {
        if (!info_.invoke)
        {
            throw std::bad_function_call();
        }
        return info_.invoke(info_.obj, std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept
    {
        return info_.invoke != nullptr;
    }

    // 获取大小
    static constexpr size_t inline_size()
    {
        return INLINE_SIZE;
    }
    bool uses_inline_storage() const
    {
        return info_.is_inline;
    }

  private:
    call_info info_;
    storage_t storage_;
};

// ================================
// 专门针对函数指针的优化版本
// ================================

template <typename Signature>
class func_ptr;

template <typename R, typename... Args>
class func_ptr<R(Args...)>
{
  private:
    using func_t = R (*)(Args...);
    func_t f_;

  public:
    func_ptr(func_t f = nullptr) noexcept : f_(f) {}

    func_ptr(const func_ptr &other) = default;
    func_ptr(func_ptr &&other) = default;
    func_ptr &operator=(const func_ptr &other) = default;
    func_ptr &operator=(func_ptr &&other) = default;

    R operator()(Args... args) const
    {
        if (!f_)
        {
            throw std::bad_function_call();
        }
        return f_(std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept
    {
        return f_ != nullptr;
    }

    static constexpr size_t size()
    {
        return sizeof(func_t);
    }
};

// ================================
// 简化版：any_func 优化版本
// ================================

template <typename Signature>
class any_func;

template <typename R, typename... Args>
class any_func<R(Args...)>
{
  private:
    // 调用信息
    struct call_info
    {
        void *obj;
        R (*invoke)(void *, Args...);
        void (*cleanup)(void *) noexcept;
        bool is_inline;
    };

    // 存储（32字节）
    static constexpr size_t STORAGE_SIZE = 32;
    union storage_t {
        alignas(8) std::byte inline_data[STORAGE_SIZE];
        void *heap_ptr;

        storage_t() noexcept : inline_data{} {}
        ~storage_t() noexcept {}
    };

  public:
    // 默认构造
    any_func() noexcept : info_{nullptr, nullptr, nullptr, false} {}

    // 构造函数
    template <typename F>
    any_func(F &&f)
    {
        using T = std::decay_t<F>;

        constexpr bool fits_inline =
            sizeof(T) <= STORAGE_SIZE && alignof(T) <= alignof(std::max_align_t);

        if constexpr (fits_inline)
        {
            // 内联存储
            new (&storage_.inline_data) T(std::forward<F>(f));
            info_.obj = &storage_.inline_data;
            info_.is_inline = true;

            // 设置调用函数
            info_.invoke = [](void *obj, Args... args) -> R {
                return (*static_cast<T *>(obj))(std::forward<Args>(args)...);
            };

            // 设置清理函数
            info_.cleanup = [](void *obj) noexcept {
                static_cast<T *>(obj)->~T();
            };
        }
        else
        {
            // 堆存储
            T *ptr = new T(std::forward<F>(f));
            storage_.heap_ptr = ptr;
            info_.obj = ptr;
            info_.is_inline = false;

            // 设置调用函数
            info_.invoke = [](void *obj, Args... args) -> R {
                return (*static_cast<T *>(obj))(std::forward<Args>(args)...);
            };

            // 设置清理函数
            info_.cleanup = [](void *obj) noexcept {
                delete static_cast<T *>(obj);
            };
        }
    }

    // 移动构造函数
    any_func(any_func &&other) noexcept : info_(other.info_), storage_(other.storage_)
    {
        other.info_ = {nullptr, nullptr, nullptr, false};
        if (info_.is_inline)
        {
            // 内联存储：移动后需要更新指针
            info_.obj = &storage_.inline_data;
        }
    }

    // 析构函数
    ~any_func()
    {
        if (info_.cleanup)
        {
            info_.cleanup(info_.obj);
        }
    }

    // 调用
    R operator()(Args... args) const
    {
        if (!info_.invoke)
        {
            throw std::bad_function_call();
        }
        return info_.invoke(info_.obj, std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept
    {
        return info_.invoke != nullptr;
    }

  private:
    call_info info_;
    storage_t storage_;
};

// ================================
// 测试用例
// ================================

struct TestFunctorSmall
{
    int value;

    int operator()(int x) const noexcept
    {
        return x + value;
    }
};

struct TestFunctorLarge
{
    std::array<char, 64> data;

    int operator()(int x) const noexcept
    {
        return x + static_cast<int>(data[0]);
    }

    TestFunctorLarge()
    {
        data.fill('A');
    }
};

int test_function(int x) noexcept
{
    return x * 2;
}

auto test_lambda = [](int x) noexcept {
    return x * 3;
};

// ================================
// 性能测试框架
// ================================

class PerformanceTest
{
  public:
    template <typename Func>
    static void run_test(const std::string &name, Func &&func, int iterations = 10000000)
    {
        // 预热
        volatile int dummy = 0;
        for (int i = 0; i < 1000; ++i)
        {
            dummy += func(i);
        }

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            dummy += func(i);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "  " << std::left << std::setw(30) << name << ": " << std::fixed
                  << std::setprecision(2)
                  << static_cast<double>(duration.count()) / iterations << " ns/call"
                  << std::endl;
    }

    static void compare_all()
    {
        constexpr int ITERATIONS = 10000000;

        std::cout << "=== ULTIMATE PERFORMANCE COMPARISON ===" << std::endl;
        std::cout << std::string(50, '=') << "\n\n";

        // 基准测试
        std::cout << "BASELINE (direct calls):\n";
        run_test("Direct function call", test_function, ITERATIONS);
        run_test("Direct lambda call", test_lambda, ITERATIONS);

        TestFunctorSmall small_functor{5};
        run_test("Direct small functor", small_functor, ITERATIONS);

        // std::function
        std::cout << "\nstd::function:\n";
        std::function<int(int)> std_func = test_function;
        std::function<int(int)> std_lambda = test_lambda;
        std::function<int(int)> std_small = TestFunctorSmall{5};
        std::function<int(int)> std_large = TestFunctorLarge{};

        run_test("Function pointer", std_func, ITERATIONS);
        run_test("Lambda", std_lambda, ITERATIONS);
        run_test("Small functor", std_small, ITERATIONS);
        run_test("Large functor", std_large, ITERATIONS);

        // fast_func
        std::cout << "\nfast_func (constructor-bound invoke):\n";
        fast_func<int(int)> fast_func_ptr = test_function;
        fast_func<int(int)> fast_lambda = test_lambda;
        fast_func<int(int)> fast_small = TestFunctorSmall{5};
        fast_func<int(int)> fast_large = TestFunctorLarge{};

        run_test("Function pointer", fast_func_ptr, ITERATIONS);
        run_test("Lambda", fast_lambda, ITERATIONS);
        run_test("Small functor", fast_small, ITERATIONS);
        run_test("Large functor", fast_large, ITERATIONS);

        // func_ptr (专门优化)
        std::cout << "\nfunc_ptr (optimized for function pointers):\n";
        func_ptr<int(int)> func_ptr_wrapper = test_function;
        run_test("Function pointer", func_ptr_wrapper, ITERATIONS);

        // any_func (通用版本)
        std::cout << "\nany_func (adaptive strategy):\n";
        any_func<int(int)> any_func_ptr = test_function;
        any_func<int(int)> any_lambda = test_lambda;
        any_func<int(int)> any_small = TestFunctorSmall{5};
        any_func<int(int)> any_large = TestFunctorLarge{};

        run_test("Function pointer", any_func_ptr, ITERATIONS);
        run_test("Lambda", any_lambda, ITERATIONS);
        run_test("Small functor", any_small, ITERATIONS);
        run_test("Large functor", any_large, ITERATIONS);

        // 内存占用
        std::cout << "\nMEMORY USAGE:\n";
        std::cout << "  std::function<int(int)>:        "
                  << sizeof(std::function<int(int)>) << " bytes\n";
        std::cout << "  fast_func<int(int)>:            " << sizeof(fast_func<int(int)>)
                  << " bytes\n";
        std::cout << "  func_ptr<int(int)>:             " << sizeof(func_ptr<int(int)>)
                  << " bytes\n";
        std::cout << "  any_func<int(int)>:             " << sizeof(any_func<int(int)>)
                  << " bytes\n";
        std::cout << "  TestFunctorSmall:               " << sizeof(TestFunctorSmall)
                  << " bytes\n";
        std::cout << "  TestFunctorLarge:               " << sizeof(TestFunctorLarge)
                  << " bytes\n";

        // 大量调用测试
        std::cout << "\nMASSIVE CALL TEST (100 million calls):\n";

        auto run_massive = [](auto &&func, const std::string &name) {
            auto start = std::chrono::high_resolution_clock::now();
            volatile int result = 0;
            for (int i = 0; i < 100000000; ++i)
            {
                result += func(i & 0xFF);
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "  " << std::left << std::setw(20) << name << ": "
                      << duration.count() << " ms\n";
        };

        std::function<int(int)> std_massive = [](int x) {
            return x * 2;
        };
        fast_func<int(int)> fast_massive = [](int x) {
            return x * 2;
        };
        any_func<int(int)> any_massive = [](int x) {
            return x * 2;
        };

        run_massive(std_massive, "std::function");
        run_massive(fast_massive, "fast_func");
        run_massive(any_massive, "any_func");
    }
};

// ================================
// 正确性测试 - 简单版本
// ================================

void test_correctness_simple()
{
    std::cout << "\nCORRECTNESS TESTS (simple):\n";

    // fast_func 测试
    {
        std::cout << "Testing fast_func:\n";
        fast_func<int(int)> f1 = test_function;
        fast_func<int(int)> f2 = test_lambda;
        fast_func<int(int)> f3 = TestFunctorSmall{10};

        std::cout << "  Function pointer: " << f1(5) << " (expected: 10)\n";
        std::cout << "  Lambda: " << f2(5) << " (expected: 15)\n";
        std::cout << "  Small functor: " << f3(5) << " (expected: 15)\n";

        // 测试大对象
        try
        {
            fast_func<int(int)> f4 = TestFunctorLarge{};
            std::cout << "  Large functor: " << f4(5) << " (expected: " << 5 + 'A'
                      << ")\n";
        }
        catch (const std::exception &e)
        {
            std::cout << "  Large functor test failed: " << e.what() << "\n";
        }
    }

    // func_ptr 测试
    {
        std::cout << "\nTesting func_ptr:\n";
        func_ptr<int(int)> f = test_function;
        std::cout << "  Function pointer: " << f(5) << " (expected: 10)\n";
    }

    // any_func 测试
    {
        std::cout << "\nTesting any_func:\n";
        any_func<int(int)> f1 = test_function;
        any_func<int(int)> f2 = test_lambda;
        any_func<int(int)> f3 = TestFunctorSmall{10};
        any_func<int(int)> f4 = TestFunctorLarge{};

        std::cout << "  Function pointer: " << f1(5) << " (expected: 10)\n";
        std::cout << "  Lambda: " << f2(5) << " (expected: 15)\n";
        std::cout << "  Small functor: " << f3(5) << " (expected: 15)\n";
        std::cout << "  Large functor: " << f4(5) << " (expected: " << 5 + 'A' << ")\n";
    }

    // 空函数测试
    std::cout << "\nTesting empty function:\n";
    try
    {
        fast_func<int(int)> empty;
        empty(5);
        std::cout << "  ERROR: Should have thrown!\n";
    }
    catch (const std::bad_function_call &)
    {
        std::cout << "  Correctly caught bad_function_call\n";
    }
}

// ================================
// 主函数
// ================================

int main()
{
    std::cout << "TESTING OPTIMIZED FUNCTION WRAPPERS\n";
    std::cout << "====================================\n\n";

    // 运行性能测试
    PerformanceTest::compare_all();

    // 运行正确性测试
    test_correctness_simple();

    std::cout << "\n====================================\n";
    std::cout << "ALL TESTS COMPLETED\n";

    return 0;
}