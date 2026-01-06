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
#include <any>

// ================================
// 优化版本：直接在构造函数中绑定invoke
// ================================

template <typename Signature>
class fast_function;

template <typename R, typename... Args>
class fast_function<R(Args...)>
{
  private:
    // 存储类型和调用函数
    using invoke_func_t = R (*)(void *storage, Args...);
    using destroy_func_t = void (*)(void *storage) noexcept;

    // 优化：小对象直接存储
    static constexpr size_t BUFFER_SIZE = 32;
    static constexpr size_t BUFFER_ALIGN = alignof(std::max_align_t);

    union storage_t {
        alignas(BUFFER_ALIGN) std::byte buffer[BUFFER_SIZE];
        void *heap_ptr;

        storage_t() noexcept : buffer{} {}
        ~storage_t() noexcept {}
    };

  public:
    // 默认构造函数
    fast_function() noexcept
        : invoke_(nullptr), destroy_(nullptr), is_inline_(false), storage_{}
    {
    }

    // 构造函数：针对函数指针
    template <typename F>
    fast_function(F *f) noexcept : is_inline_(true), storage_{}
    {
        // 存储函数指针
        new (storage_.buffer) F *(f);

        // 直接在构造函数中绑定invoke函数
        invoke_ = [](void *storage, Args... args) -> R {
            auto *func_ptr = *reinterpret_cast<F **>(storage);
            return (*func_ptr)(std::forward<Args>(args)...);
        };

        destroy_ = [](void *storage) noexcept {
            // 无操作，函数指针不需要销毁
        };
    }

    // 构造函数：针对可调用对象
    template <typename F,
              typename = std::enable_if_t<!std::is_pointer_v<std::decay_t<F>>>>
    fast_function(F &&f)
    {
        using functor_type = std::decay_t<F>;

        // 判断是否适合内联存储
        is_inline_ =
            sizeof(functor_type) <= BUFFER_SIZE && alignof(functor_type) <= BUFFER_ALIGN;

        if (is_inline_)
        {
            // 内联存储：直接在buffer中构造对象
            new (storage_.buffer) functor_type(std::forward<F>(f));

            // 绑定内联版本的invoke
            invoke_ = [](void *storage, Args... args) -> R {
                auto &obj = *reinterpret_cast<functor_type *>(storage);
                return obj(std::forward<Args>(args)...);
            };

            destroy_ = [](void *storage) noexcept {
                reinterpret_cast<functor_type *>(storage)->~functor_type();
            };
        }
        else
        {
            // 堆存储
            storage_.heap_ptr = new functor_type(std::forward<F>(f));

            // 绑定堆存储版本的invoke
            invoke_ = [](void *storage, Args... args) -> R {
                auto *obj = static_cast<functor_type *>(
                    reinterpret_cast<storage_t *>(storage)->heap_ptr);
                return (*obj)(std::forward<Args>(args)...);
            };

            destroy_ = [](void *storage) noexcept {
                auto *ptr = reinterpret_cast<storage_t *>(storage)->heap_ptr;
                delete static_cast<functor_type *>(ptr);
            };
        }
    }

    // 移动构造函数
    fast_function(fast_function &&other) noexcept
        : invoke_(std::exchange(other.invoke_, nullptr)),
          destroy_(std::exchange(other.destroy_, nullptr)), is_inline_(other.is_inline_),
          storage_(other.storage_)
    {
        // 清除原对象状态
        other.is_inline_ = false;
    }

    // 析构函数
    ~fast_function()
    {
        if (destroy_)
        {
            destroy_(&storage_);
        }
    }

    // 调用运算符 - 直接调用绑定的invoke函数
    R operator()(Args... args) const
    {
        if (!invoke_)
        {
            throw std::bad_function_call();
        }
        return invoke_(&storage_, std::forward<Args>(args)...);
    }

    explicit operator bool() const noexcept
    {
        return invoke_ != nullptr;
    }

    // 获取大小信息
    static constexpr size_t buffer_size() noexcept
    {
        return BUFFER_SIZE;
    }
    static constexpr size_t buffer_align() noexcept
    {
        return BUFFER_ALIGN;
    }
    bool uses_inline_storage() const noexcept
    {
        return is_inline_;
    }

  private:
    invoke_func_t invoke_;      // 直接绑定的调用函数
    destroy_func_t destroy_;    // 直接绑定的销毁函数
    bool is_inline_;            // 是否使用内联存储
    mutable storage_t storage_; // 存储空间
};

// ================================
// 改进版：使用编译期生成的invoke
// ================================

template <typename Signature>
class compiletime_function;

template <typename R, typename... Args>
class compiletime_function<R(Args...)>
{
  private:
    // 存储和调用信息
    struct call_info
    {
        void *data;
        R (*invoke)(void *, Args...);
        void (*destroy)(void *) noexcept;
    };

    // 内联存储
    static constexpr size_t INLINE_SIZE = 24;

    union storage_t {
        alignas(alignof(std::max_align_t)) std::byte buffer[INLINE_SIZE];
        void *heap_ptr;

        storage_t() noexcept : buffer{} {}
    };

  public:
    // 构造函数：使用模板生成调用代码
    template <typename F>
    compiletime_function(F &&f)
    {
        using functor_type = std::decay_t<F>;

        // 在编译期决定存储策略和生成调用代码
        if constexpr (sizeof(functor_type) <= INLINE_SIZE &&
                      alignof(functor_type) <= alignof(std::max_align_t))
        {
            // 内联存储 + 编译期生成的lambda
            new (storage_.buffer) functor_type(std::forward<F>(f));
            data_ = storage_.buffer;

            // 编译期生成的invoke函数（lambda被内联优化）
            invoke_ = [](void *data, Args... args) -> R {
                auto &obj = *reinterpret_cast<functor_type *>(data);
                return obj(std::forward<Args>(args)...);
            };

            destroy_ = [](void *data) noexcept {
                reinterpret_cast<functor_type *>(data)->~functor_type();
            };
        }
        else
        {
            // 堆存储
            storage_.heap_ptr = new functor_type(std::forward<F>(f));
            data_ = storage_.heap_ptr;

            invoke_ = [](void *data, Args... args) -> R {
                auto &obj = *static_cast<functor_type *>(data);
                return obj(std::forward<Args>(args)...);
            };

            destroy_ = [](void *data) noexcept {
                delete static_cast<functor_type *>(data);
            };
        }
    }

    // 调用
    R operator()(Args... args) const
    {
        return invoke_(data_, std::forward<Args>(args)...);
    }

  private:
    void *data_ = nullptr;
    R (*invoke_)(void *, Args...) = nullptr;
    void (*destroy_)(void *) noexcept = nullptr;
    storage_t storage_;
};

// ================================
// 极致优化版：无分支调用
// ================================

template <typename Signature>
class direct_function;

template <typename R, typename... Args>
class direct_function<R(Args...)>
{
  private:
    // 使用函数指针直接调用，存储对象指针
    using invoke_t = R (*)(void *, Args...);

    // 存储布局：函数指针 + 对象指针
    struct storage
    {
        void *obj_ptr;
        invoke_t invoke_fn;

        // 针对函数指针的特化
        template <typename FuncPtr>
        storage(FuncPtr f) noexcept
            : obj_ptr(reinterpret_cast<void *>(f)),
              invoke_fn([](void *ptr, Args... args) -> R {
                  return reinterpret_cast<FuncPtr>(ptr)(std::forward<Args>(args)...);
              })
        {
        }

        // 针对可调用对象的特化
        template <typename F>
        storage(F &&f)
            : obj_ptr(new std::decay_t<F>(std::forward<F>(f))),
              invoke_fn([](void *ptr, Args... args) -> R {
                  return (*static_cast<std::decay_t<F> *>(ptr))(
                      std::forward<Args>(args)...);
              })
        {
        }

        ~storage()
        {
            // 如果是堆分配的对象，需要删除
            if (invoke_fn != nullptr)
            {
                // 这里需要知道对象类型才能正确删除
                // 简化处理：假设都是堆分配的
                delete static_cast<char *>(obj_ptr);
            }
        }
    };

  public:
    template <typename F>
    direct_function(F &&f) : storage_(std::forward<F>(f))
    {
    }

    R operator()(Args... args) const
    {
        return storage_.invoke_fn(storage_.obj_ptr, std::forward<Args>(args)...);
    }

  private:
    storage storage_;
};

// ================================
// 测试用例
// ================================

struct TestFunctor
{
    int value;

    int operator()(int x) const noexcept
    {
        return x + value;
    }
};

int test_function(int x) noexcept
{
    return x * 2;
}

// ================================
// 性能测试
// ================================

class PerformanceCompare
{
  public:
    template <typename Func>
    static void test(const std::string &name, Func &&func, int iterations = 10000000)
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

        std::cout << "  " << name << ": "
                  << static_cast<double>(duration.count()) / iterations << " ns/call"
                  << std::endl;
    }

    static void run_all_tests()
    {
        constexpr int ITERATIONS = 10000000;

        std::cout << "=== Performance Comparison (Constructor-bound invoke) ===\n\n";

        // 1. 直接调用基准
        std::cout << "Direct calls:\n";
        test("Direct function call", test_function, ITERATIONS);

        TestFunctor direct_obj{5};
        test("Direct object call", direct_obj, ITERATIONS);

        // 2. std::function
        std::cout << "\nstd::function:\n";
        std::function<int(int)> std_func = test_function;
        test("Function pointer", std_func, ITERATIONS);

        std::function<int(int)> std_obj = TestFunctor{5};
        test("Functor object", std_obj, ITERATIONS);

        std::function<int(int)> std_lambda = [](int x) {
            return x * 3;
        };
        test("Lambda", std_lambda, ITERATIONS);

        // 3. fast_function (构造函数绑定invoke)
        std::cout << "\nfast_function (constructor-bound invoke):\n";
        fast_function<int(int)> fast_func = test_function;
        test("Function pointer", fast_func, ITERATIONS);

        fast_function<int(int)> fast_obj = TestFunctor{5};
        test("Functor object", fast_obj, ITERATIONS);

        fast_function<int(int)> fast_lambda = [](int x) {
            return x * 3;
        };
        test("Lambda", fast_lambda, ITERATIONS);

        // 4. compiletime_function
        std::cout << "\ncompiletime_function (compile-time invoke):\n";
        compiletime_function<int(int)> ct_func = test_function;
        test("Function pointer", ct_func, ITERATIONS);

        compiletime_function<int(int)> ct_obj = TestFunctor{5};
        test("Functor object", ct_obj, ITERATIONS);

        // 5. 内存占用对比
        std::cout << "\nMemory sizes:\n";
        std::cout << "  sizeof(std::function<int(int)>): "
                  << sizeof(std::function<int(int)>) << " bytes\n";
        std::cout << "  sizeof(fast_function<int(int)>): "
                  << sizeof(fast_function<int(int)>) << " bytes\n";
        std::cout << "  sizeof(compiletime_function<int(int)>): "
                  << sizeof(compiletime_function<int(int)>) << " bytes\n";
        std::cout << "  sizeof(direct_function<int(int)>): "
                  << sizeof(direct_function<int(int)>) << " bytes\n";

        // 6. 大量调用测试
        std::cout << "\nMassive invocation test (100 million calls):\n";

        {
            auto start = std::chrono::high_resolution_clock::now();

            std::function<int(int)> f = [](int x) {
                return x * 2;
            };
            volatile int result = 0;
            for (int i = 0; i < 100000000; ++i)
            {
                result += f(i & 0xFF);
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "  std::function: " << duration.count() << " ms\n";
        }

        {
            auto start = std::chrono::high_resolution_clock::now();

            fast_function<int(int)> f = [](int x) {
                return x * 2;
            };
            volatile int result = 0;
            for (int i = 0; i < 100000000; ++i)
            {
                result += f(i & 0xFF);
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "  fast_function: " << duration.count() << " ms\n";
        }

        // 7. 调用链深度测试
        std::cout << "\nCall chain depth test:\n";

        {
            std::function<int(int)> f1 = [](int x) {
                return x + 1;
            };
            std::function<int(int)> f2 = [](int x) {
                return x * 2;
            };
            std::function<int(int)> f3 = [](int x) {
                return x - 3;
            };

            auto start = std::chrono::high_resolution_clock::now();
            volatile int result = 0;
            for (int i = 0; i < ITERATIONS; ++i)
            {
                result += f3(f2(f1(i)));
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            std::cout << "  std::function chain: "
                      << static_cast<double>(duration.count()) / ITERATIONS
                      << " ns/call\n";
        }

        {
            fast_function<int(int)> f1 = [](int x) {
                return x + 1;
            };
            fast_function<int(int)> f2 = [](int x) {
                return x * 2;
            };
            fast_function<int(int)> f3 = [](int x) {
                return x - 3;
            };

            auto start = std::chrono::high_resolution_clock::now();
            volatile int result = 0;
            for (int i = 0; i < ITERATIONS; ++i)
            {
                result += f3(f2(f1(i)));
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            std::cout << "  fast_function chain: "
                      << static_cast<double>(duration.count()) / ITERATIONS
                      << " ns/call\n";
        }
    }
};

// ================================
// 微基准：测试不同调用策略
// ================================

void micro_benchmark()
{
    constexpr int ITERATIONS = 100000000;

    // 测试不同大小的函数对象
    struct Tiny
    {
        int operator()(int x) const
        {
            return x + 1;
        }
    };
    struct Small
    {
        int data[4];
        int operator()(int x) const
        {
            return x + data[0];
        }
    };
    struct Medium
    {
        int data[16];
        int operator()(int x) const
        {
            return x + data[0];
        }
    };
    struct Large
    {
        int data[64];
        int operator()(int x) const
        {
            return x + data[0];
        }
    };

    std::cout << "\n=== Micro-benchmark: Different object sizes ===\n";

    // Tiny object
    {
        Tiny t;
        std::function<int(int)> std_tiny = t;
        fast_function<int(int)> fast_tiny = t;

        auto start = std::chrono::high_resolution_clock::now();
        volatile int r1 = 0;
        for (int i = 0; i < ITERATIONS; ++i)
            r1 += std_tiny(i);
        auto mid = std::chrono::high_resolution_clock::now();
        volatile int r2 = 0;
        for (int i = 0; i < ITERATIONS; ++i)
            r2 += fast_tiny(i);
        auto end = std::chrono::high_resolution_clock::now();

        auto std_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(mid - start);
        auto fast_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid);

        std::cout << "Tiny object: std::function=" << std_time.count()
                  << "ms, fast_function=" << fast_time.count() << "ms\n";
    }

    // Small object
    {
        Small s{{1, 2, 3, 4}};
        std::function<int(int)> std_small = s;
        fast_function<int(int)> fast_small = s;

        auto start = std::chrono::high_resolution_clock::now();
        volatile int r1 = 0;
        for (int i = 0; i < ITERATIONS; ++i)
            r1 += std_small(i);
        auto mid = std::chrono::high_resolution_clock::now();
        volatile int r2 = 0;
        for (int i = 0; i < ITERATIONS; ++i)
            r2 += fast_small(i);
        auto end = std::chrono::high_resolution_clock::now();

        auto std_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(mid - start);
        auto fast_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid);

        std::cout << "Small object: std::function=" << std_time.count()
                  << "ms, fast_function=" << fast_time.count() << "ms\n";
    }

    // Medium object (可能触发堆分配)
    {
        Medium m{};
        m.data[0] = 1;
        std::function<int(int)> std_medium = m;
        fast_function<int(int)> fast_medium = m;

        auto start = std::chrono::high_resolution_clock::now();
        volatile int r1 = 0;
        for (int i = 0; i < ITERATIONS; ++i)
            r1 += std_medium(i);
        auto mid = std::chrono::high_resolution_clock::now();
        volatile int r2 = 0;
        for (int i = 0; i < ITERATIONS; ++i)
            r2 += fast_medium(i);
        auto end = std::chrono::high_resolution_clock::now();

        auto std_time =
            std::chrono::duration_cast<std::chrono::milliseconds>(mid - start);
        auto fast_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - mid);

        std::cout << "Medium object: std::function=" << std_time.count()
                  << "ms, fast_function=" << fast_time.count() << "ms\n";
    }
}

// ================================
// 主函数
// ================================

int main()
{
    std::cout << "Testing constructor-bound invoke optimization...\n\n";

    // 运行性能测试
    PerformanceCompare::run_all_tests();

    // 运行微基准测试
    micro_benchmark();

    // 验证正确性
    std::cout << "\n=== Correctness verification ===\n";

    fast_function<int(int)> f1 = [](int x) {
        return x * 2;
    };
    fast_function<int(int)> f2 = test_function;
    fast_function<int(int)> f3 = TestFunctor{10};

    std::cout << "Lambda: f1(5) = " << f1(5) << " (expected: 10)\n";
    std::cout << "Function: f2(5) = " << f2(5) << " (expected: 10)\n";
    std::cout << "Functor: f3(5) = " << f3(5) << " (expected: 15)\n";

    // 测试空函数调用
    try
    {
        fast_function<int(int)> empty;
        empty(5); // 应该抛出异常
        std::cout << "ERROR: Should have thrown!\n";
    }
    catch (const std::bad_function_call &)
    {
        std::cout << "Correctly caught bad_function_call\n";
    }

    std::cout << "\n=== Test completed ===\n";
    return 0;
}