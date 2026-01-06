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

// NOLINTBEGIN
// ================================
// 基础类型擦除框架
// ================================

template <typename Signature>
class function_like;

template <typename R, typename... Args>
class function_like<R(Args...)>
{
  private:
    // 手写虚表结构
    struct vtable
    {
        R (*invoke)(void *storage, Args... args);
        void (*destroy)(void *storage) noexcept;
        void (*move)(void *dest, void *src) noexcept;
        void (*copy)(void *dest, const void *src);
        size_t (*size)() noexcept;
    };

    // 存储联合体，支持 SSO
    union storage_t {
        alignas(std::max_align_t) std::byte stack_buffer[32];
        void *heap_ptr;
    };

    static constexpr size_t STACK_SIZE = 32;
    static constexpr size_t STACK_ALIGN = alignof(std::max_align_t);

  public:
    // 默认构造函数
    function_like() noexcept : vtable_(nullptr), storage_{} {}

    // 构造函数：接受任意可调用对象
    template <typename F>
    function_like(F &&f) : vtable_(get_vtable<std::decay_t<F>>())
    {
        using functor_type = std::decay_t<F>;

        if constexpr (fits_in_stack<functor_type>())
        {
            // 小对象：存储在栈上
            new (&storage_.stack_buffer) functor_type(std::forward<F>(f));
        }
        else
        {
            // 大对象：存储在堆上
            storage_.heap_ptr = new functor_type(std::forward<F>(f));
        }
    }

    // 复制构造函数
    function_like(const function_like &other) : vtable_(other.vtable_)
    {
        if (other)
        {
            storage_t temp{};
            vtable_->copy(&temp, &other.storage_);
            storage_ = temp;
        }
    }

    // 移动构造函数
    function_like(function_like &&other) noexcept : vtable_(other.vtable_)
    {
        if (other)
        {
            storage_t temp{};
            vtable_->move(&temp, &other.storage_);
            storage_ = temp;
            other.vtable_ = nullptr;
        }
    }

    // 析构函数
    ~function_like()
    {
        if (vtable_)
        {
            vtable_->destroy(&storage_);
        }
    }

    // 调用运算符
    R operator()(Args... args) const
    {
        if (!vtable_)
        {
            throw std::bad_function_call();
        }
        return vtable_->invoke(const_cast<void *>(static_cast<const void *>(&storage_)),
                               std::forward<Args>(args)...);
    }

    // 检查是否为空
    explicit operator bool() const noexcept
    {
        return vtable_ != nullptr;
    }

    // 交换
    void swap(function_like &other) noexcept
    {
        std::swap(vtable_, other.vtable_);
        std::swap(storage_, other.storage_);
    }

  private:
    // 判断对象是否适合栈存储
    template <typename T>
    static constexpr bool fits_in_stack() noexcept
    {
        return sizeof(T) <= STACK_SIZE && alignof(T) <= STACK_ALIGN;
    }

    // 获取指针（根据存储位置）
    template <typename T>
    static T *get_pointer(void *storage) noexcept
    {
        if constexpr (fits_in_stack<T>())
        {
            return reinterpret_cast<T *>(
                &static_cast<storage_t *>(storage)->stack_buffer);
        }
        else
        {
            return static_cast<T *>(static_cast<storage_t *>(storage)->heap_ptr);
        }
    }

    template <typename T>
    static const T *get_pointer(const void *storage) noexcept
    {
        if constexpr (fits_in_stack<T>())
        {
            return reinterpret_cast<const T *>(
                &static_cast<const storage_t *>(storage)->stack_buffer);
        }
        else
        {
            return static_cast<const T *>(
                static_cast<const storage_t *>(storage)->heap_ptr);
        }
    }

    // 为特定类型生成虚表
    template <typename T>
    static const vtable *get_vtable() noexcept
    {
        static const vtable vt = {
            .invoke = [](void *storage, Args... args) -> R {
                T *obj = get_pointer<T>(storage);
                if constexpr (std::is_void_v<R>)
                {
                    (*obj)(std::forward<Args>(args)...);
                }
                else
                {
                    return (*obj)(std::forward<Args>(args)...);
                }
            },
            .destroy =
                [](void *storage) noexcept {
                    T *obj = get_pointer<T>(storage);
                    if constexpr (fits_in_stack<T>())
                    {
                        obj->~T();
                    }
                    else
                    {
                        delete obj;
                    }
                },
            .move =
                [](void *dest, void *src) noexcept {
                    if constexpr (fits_in_stack<T>())
                    {
                        T *src_obj = get_pointer<T>(src);
                        T *dest_obj = get_pointer<T>(dest);
                        new (dest_obj) T(std::move(*src_obj));
                        src_obj->~T();
                    }
                    else
                    {
                        static_cast<storage_t *>(dest)->heap_ptr = std::exchange(
                            static_cast<storage_t *>(src)->heap_ptr, nullptr);
                    }
                },
            .copy =
                [](void *dest, const void *src) {
                    const T *src_obj = get_pointer<T>(src);
                    if constexpr (fits_in_stack<T>())
                    {
                        T *dest_obj = get_pointer<T>(dest);
                        new (dest_obj) T(*src_obj);
                    }
                    else
                    {
                        static_cast<storage_t *>(dest)->heap_ptr = new T(*src_obj);
                    }
                },
            .size = []() noexcept -> size_t { return sizeof(T); }};
        return &vt;
    }

  private:
    const vtable *vtable_;
    mutable storage_t storage_;
};

// ================================
// 测试用例
// ================================

// 小型函数对象（适合栈存储）
struct SmallFunctor
{
    int value;

    int operator()(int x) const noexcept
    {
        return x + value;
    }
};

// 中型函数对象（适合栈存储）
struct MediumFunctor
{
    std::array<char, 24> data;

    char operator()() const noexcept
    {
        return data[0];
    }
};

// 大型函数对象（需要堆存储）
struct LargeFunctor
{
    std::array<char, 64> data;

    char operator()() const noexcept
    {
        return data[0];
    }
};

// 普通函数
int add_five(int x) noexcept
{
    return x + 5;
}

// ================================
// 性能测试框架
// ================================

class PerformanceBenchmark
{
  public:
    // 用于接受int参数的函数
    template <typename Func>
    static void run_benchmark_int(const std::string &name, Func &&func,
                                  int iterations = 1000000)
    {
        auto start = std::chrono::high_resolution_clock::now();

        volatile int result = 0;
        for (int i = 0; i < iterations; ++i)
        {
            result += func(i);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "  " << name << " - " << duration.count() << " ns total, "
                  << static_cast<double>(duration.count()) / iterations
                  << " ns/iter, dummy=" << result << "\n";
    }

    // 用于无参函数的测试
    template <typename Func>
    static void run_benchmark_void(const std::string &name, Func &&func,
                                   int iterations = 1000000)
    {
        auto start = std::chrono::high_resolution_clock::now();

        volatile int result = 0;
        for (int i = 0; i < iterations; ++i)
        {
            result += static_cast<int>(func());
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "  " << name << " - " << duration.count() << " ns total, "
                  << static_cast<double>(duration.count()) / iterations
                  << " ns/iter, dummy=" << result << "\n";
    }

    static void print_memory_usage()
    {
        std::cout << "\nMemory usage comparison:\n";
        std::cout << "  sizeof(std::function<int(int)>) = "
                  << sizeof(std::function<int(int)>) << " bytes\n";
        std::cout << "  sizeof(function_like<int(int)>) = "
                  << sizeof(function_like<int(int)>) << " bytes\n";
        std::cout << "  sizeof(SmallFunctor) = " << sizeof(SmallFunctor) << " bytes\n";
        std::cout << "  sizeof(MediumFunctor) = " << sizeof(MediumFunctor) << " bytes\n";
        std::cout << "  sizeof(LargeFunctor) = " << sizeof(LargeFunctor) << " bytes\n";
    }
};

// ================================
// 编译期特性测试
// ================================

template <typename T>
void test_compile_time_properties()
{
    std::cout << "\nCompile-time test: " << typeid(T).name() << "\n";

    // 测试是否适合栈存储
    constexpr bool fits = sizeof(T) <= 32 && alignof(T) <= alignof(std::max_align_t);
    std::cout << "  Fits in stack: " << (fits ? "YES" : "NO") << "\n";

    // 测试是否为平凡类型
    std::cout << "  Trivially copyable: "
              << (std::is_trivially_copyable_v<T> ? "YES" : "NO") << "\n";

    // 测试是否无抛出
    std::cout << "  Size: " << sizeof(T) << " bytes\n";
    std::cout << "  Alignment: " << alignof(T) << " bytes\n";
}

// ================================
// 主测试函数
// ================================

int main()
{
    std::cout << "=== Handwritten vtable vs std::function Performance Test ===\n\n";

    // 1. 内存占用对比
    PerformanceBenchmark::print_memory_usage();

    // 2. 编译期特性测试
    std::cout << "\nCompile-time property analysis:\n";
    test_compile_time_properties<SmallFunctor>();
    test_compile_time_properties<MediumFunctor>();
    test_compile_time_properties<LargeFunctor>();

    constexpr int ITERATIONS = 10000000;

    // 3. 直接调用基准（作为参考）
    std::cout << "\nDirect call benchmark (reference):\n";
    PerformanceBenchmark::run_benchmark_int("Direct function call", add_five, ITERATIONS);

    SmallFunctor small{10};
    PerformanceBenchmark::run_benchmark_int("Direct object call", small, ITERATIONS);

    // 4. std::function 测试
    std::cout << "\nstd::function tests:\n";

    // 小对象
    std::function<int(int)> std_func_small = SmallFunctor{10};
    PerformanceBenchmark::run_benchmark_int("std::function - small object",
                                            std_func_small, ITERATIONS);

    // 函数指针
    std::function<int(int)> std_func_ptr = add_five;
    PerformanceBenchmark::run_benchmark_int("std::function - function pointer",
                                            std_func_ptr, ITERATIONS);

    // Lambda
    std::function<int(int)> std_func_lambda = [value = 42](int x) {
        return x * value;
    };
    PerformanceBenchmark::run_benchmark_int("std::function - Lambda", std_func_lambda,
                                            ITERATIONS);

    // 大对象（无参）
    std::function<char()> std_func_large = LargeFunctor{};
    PerformanceBenchmark::run_benchmark_void("std::function - large object (no args)",
                                             std_func_large, ITERATIONS);

    // 5. function_like 测试
    std::cout << "\nfunction_like (handwritten vtable) tests:\n";

    // 小对象
    function_like<int(int)> custom_func_small = SmallFunctor{10};
    PerformanceBenchmark::run_benchmark_int("function_like - small object",
                                            custom_func_small, ITERATIONS);

    // 函数指针
    function_like<int(int)> custom_func_ptr = add_five;
    PerformanceBenchmark::run_benchmark_int("function_like - function pointer",
                                            custom_func_ptr, ITERATIONS);

    // Lambda
    function_like<int(int)> custom_func_lambda = [value = 42](int x) {
        return x * value;
    };
    PerformanceBenchmark::run_benchmark_int("function_like - Lambda", custom_func_lambda,
                                            ITERATIONS);

    // 大对象（无参）
    function_like<char()> custom_func_large = LargeFunctor{};
    PerformanceBenchmark::run_benchmark_void("function_like - large object (no args)",
                                             custom_func_large, ITERATIONS);

    // 6. 高级特性测试
    std::cout << "\nAdvanced feature tests:\n";

    // 移动语义测试
    {
        auto start = std::chrono::high_resolution_clock::now();

        std::vector<function_like<int(int)>> vec;
        vec.reserve(1000);

        for (int i = 0; i < 1000; ++i)
        {
            vec.emplace_back(SmallFunctor{i});
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  function_like move construct 1000 times: " << duration.count()
                  << " us\n";
    }

    // 复制语义测试
    {
        function_like<int(int)> original = SmallFunctor{42};

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < 1000; ++i)
        {
            function_like<int(int)> copy = original;
            volatile int dummy = copy(i);
            (void)dummy;
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "  function_like copy construct 1000 times: " << duration.count()
                  << " us\n";
    }

    // 7. 混合调用测试
    std::cout << "\nMixed call pattern tests:\n";

    // 创建混合函数数组（都有int参数）
    std::array<function_like<int(int)>, 4> mixed_funcs = {
        SmallFunctor{1}, add_five, [](int x) { return x * 2; },
        [value = 10](int x) { return x + value; }};

    std::array<std::function<int(int)>, 4> std_mixed_funcs = {
        SmallFunctor{1}, add_five, [](int x) { return x * 2; },
        [value = 10](int x) { return x + value; }};

    // 手写虚表版本
    {
        auto start = std::chrono::high_resolution_clock::now();

        volatile int result = 0;
        for (int i = 0; i < ITERATIONS / 4; ++i)
        {
            for (size_t j = 0; j < mixed_funcs.size(); ++j)
            {
                result += mixed_funcs[j](i);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "  function_like mixed calls: "
                  << duration.count() / (ITERATIONS / 1000.0) << " ns/1000 calls\n";
    }

    // std::function 版本
    {
        auto start = std::chrono::high_resolution_clock::now();

        volatile int result = 0;
        for (int i = 0; i < ITERATIONS / 4; ++i)
        {
            for (size_t j = 0; j < std_mixed_funcs.size(); ++j)
            {
                result += std_mixed_funcs[j](i);
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "  std::function mixed calls: "
                  << duration.count() / (ITERATIONS / 1000.0) << " ns/1000 calls\n";
    }

    // 8. 调用开销分析
    std::cout << "\nCall overhead analysis (10 million calls):\n";

    // 虚函数调用开销（作为比较）
    struct Base
    {
        virtual int call(int x)
        {
            return x;
        }
        virtual ~Base() = default;
    };

    struct Derived : Base
    {
        int call(int x) override
        {
            return x * 2;
        }
    };

    {
        Derived d;
        Base *ptr = &d;

        auto start = std::chrono::high_resolution_clock::now();
        volatile int result = 0;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            result += ptr->call(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "  Virtual function call: "
                  << static_cast<double>(duration.count()) / ITERATIONS << " ns/call\n";
    }

    // 函数指针调用开销
    {
        int (*func_ptr)(int) = add_five;

        auto start = std::chrono::high_resolution_clock::now();
        volatile int result = 0;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            result += func_ptr(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "  Function pointer call: "
                  << static_cast<double>(duration.count()) / ITERATIONS << " ns/call\n";
    }

    std::cout << "\n=== Test completed ===\n";

    return 0;
}
// NOLINTEND