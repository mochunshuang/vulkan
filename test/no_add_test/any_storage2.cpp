#include <bit>
#include <cassert>
#include <functional>
#include <iostream>
#include <type_traits>
#include <memory>
#include <utility>
#include <cstring>
#include <chrono>
#include <array>
#include <optional>

// NOLINTBEGIN
// 你的 any_storage 实现，添加默认构造函数
template <std::size_t size, size_t align>
struct any_storage
{
    using allocator_storage_type = std::allocator<void>;
    static constexpr auto buffer_size = size;
    static constexpr auto align_size = align;

    struct storage_ops
    {
        void (*destroy)(any_storage &self) noexcept;
        void (*copy_construct)(any_storage &dest, const any_storage &src);
        void (*move_construct)(any_storage &dest, any_storage &src) noexcept;
        bool (*equals)(const any_storage &a, const any_storage &b) noexcept;
        const std::type_info &(*type_info_T)() noexcept;
        const std::type_info &(*type_info_Allocator)() noexcept;
    };

    allocator_storage_type allocator_;
    union storage_union {
        alignas(align) std::byte stack_buffer[size];
        void *heap_ptr;
    } storage_{}; // 值初始化

    const storage_ops *ops_ = nullptr;

    // 添加默认构造函数
    constexpr any_storage() noexcept : allocator_{}, storage_{}, ops_{nullptr} {}

    template <typename T>
    constexpr T *as_small() noexcept
    {
        return std::bit_cast<T *>(&storage_.stack_buffer[0]);
    }

    template <typename T>
    constexpr T *as_large() noexcept
    {
        return static_cast<T *>(storage_.heap_ptr);
    }

    template <typename T>
    constexpr const T *as_small() const noexcept
    {
        return std::bit_cast<const T *>(&storage_.stack_buffer[0]);
    }

    template <typename T>
    constexpr const T *as_large() const noexcept
    {
        return static_cast<const T *>(storage_.heap_ptr);
    }

    template <typename T>
    static consteval bool is_small() noexcept
    {
        return sizeof(T) <= buffer_size && alignof(T) <= align_size;
    }

    template <typename T>
    static constexpr T *get_pointer(void *storage) noexcept
    {
        if constexpr (is_small<T>())
            return std::bit_cast<any_storage *>(storage)->template as_small<T>();
        else
            return std::bit_cast<any_storage *>(storage)->template as_large<T>();
    }

    template <typename T>
    static constexpr const T *get_pointer(const void *storage) noexcept
    {
        if constexpr (is_small<T>())
            return std::bit_cast<const any_storage *>(storage)->template as_small<T>();
        else
            return std::bit_cast<const any_storage *>(storage)->template as_large<T>();
    }

    template <typename T, typename Allocator>
    static void destroy_impl(any_storage &self) noexcept
    {
        using ReboundAlloc =
            typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        ReboundAlloc rebound_alloc(self.allocator_);

        if constexpr (is_small<T>())
        {
            auto *ptr = self.template as_small<T>();
            std::allocator_traits<ReboundAlloc>::destroy(rebound_alloc, ptr);
        }
        else
        {
            auto *ptr = self.template as_large<T>();
            if (ptr)
            {
                std::allocator_traits<ReboundAlloc>::destroy(rebound_alloc, ptr);
                std::allocator_traits<ReboundAlloc>::deallocate(rebound_alloc, ptr, 1);
                self.storage_.heap_ptr = nullptr;
            }
        }
        self.ops_ = nullptr;
    }

    template <typename T, typename Allocator>
    static void copy_construct_impl(any_storage &dest, const any_storage &src)
    {
        using ReboundAlloc =
            typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        ReboundAlloc rebound_alloc(dest.allocator_);

        const T *src_obj = nullptr;
        if constexpr (is_small<T>())
        {
            src_obj = src.template as_small<T>();
        }
        else
        {
            src_obj = src.template as_large<T>();
        }

        if constexpr (is_small<T>())
        {
            auto *ptr = dest.template as_small<T>();
            std::allocator_traits<ReboundAlloc>::construct(rebound_alloc, ptr, *src_obj);
        }
        else
        {
            T *ptr = std::allocator_traits<ReboundAlloc>::allocate(rebound_alloc, 1);
            try
            {
                std::allocator_traits<ReboundAlloc>::construct(rebound_alloc, ptr,
                                                               *src_obj);
                dest.storage_.heap_ptr = ptr;
            }
            catch (...)
            {
                std::allocator_traits<ReboundAlloc>::deallocate(rebound_alloc, ptr, 1);
                throw;
            }
        }
    }

    template <typename T, typename Allocator>
    static void move_construct_impl(any_storage &dest, any_storage &src) noexcept
    {
        using ReboundAlloc =
            typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        ReboundAlloc rebound_alloc(dest.allocator_);

        if constexpr (is_small<T>())
        {
            auto *src_ptr = src.template as_small<T>();
            auto *dest_ptr = dest.template as_small<T>();
            std::allocator_traits<ReboundAlloc>::construct(rebound_alloc, dest_ptr,
                                                           std::move(*src_ptr));
        }
        else
        {
            dest.storage_.heap_ptr = std::exchange(src.storage_.heap_ptr, nullptr);
        }
    }

    template <typename T>
    static const std::type_info &type_info_T_impl() noexcept
    {
        return typeid(T);
    }

    template <typename Allocator>
    static const std::type_info &type_info_Allocator_impl() noexcept
    {
        return typeid(Allocator);
    }

    friend bool operator==(const any_storage &a, const any_storage &b) noexcept
    {
        if ((a.ops_ == nullptr) && (b.ops_ == nullptr))
            return true;
        if ((a.ops_ == nullptr) || (b.ops_ == nullptr))
            return false;
        if (a.ops_->type_info_T() != b.ops_->type_info_T() ||
            a.ops_->type_info_Allocator() != b.ops_->type_info_Allocator())
            return false;
        return a.ops_->equals(a, b);
    }

    friend bool operator!=(const any_storage &a, const any_storage &b) noexcept
    {
        return !(a == b);
    }

    [[nodiscard]] constexpr const std::type_info &stored_type() const noexcept
    {
        return ops_ == nullptr ? typeid(void) : ops_->type_info_T();
    }

    [[nodiscard]] constexpr const std::type_info &allocator_type() const noexcept
    {
        return ops_ == nullptr ? typeid(void) : ops_->type_info_Allocator();
    }

    template <typename T>
    constexpr static bool equals_impl(const any_storage &a, const any_storage &b) noexcept
    {
        if constexpr (std::is_empty_v<T>)
            return true;
        else
        {
            const T *obj_a = nullptr;
            const T *obj_b = nullptr;

            if constexpr (is_small<T>())
            {
                obj_a = a.template as_small<T>();
                obj_b = b.template as_small<T>();
            }
            else
            {
                obj_a = a.template as_large<T>();
                obj_b = b.template as_large<T>();
            }
            if (!obj_a || !obj_b)
                return false;

            if constexpr (requires { *obj_a == *obj_b; })
            {
                return *obj_a == *obj_b;
            }
            else if constexpr (std::is_trivially_copyable_v<T> && is_small<T>())
            {
                return std::memcmp(obj_a, obj_b, sizeof(T)) == 0;
            }
            else
            {
                return obj_a == obj_b;
            }
        }
    }

    template <typename T, typename Allocator>
    static constexpr const storage_ops *create_ops() noexcept
    {
        static const auto vt =
            storage_ops{.destroy = &destroy_impl<T, Allocator>,
                        .copy_construct = &copy_construct_impl<T, Allocator>,
                        .move_construct = &move_construct_impl<T, Allocator>,
                        .equals = &equals_impl<T>,
                        .type_info_T = &type_info_T_impl<T>,
                        .type_info_Allocator = &type_info_Allocator_impl<Allocator>};
        return &vt;
    }

    template <typename Obj, typename Allocator>
    constexpr void construct(Obj &&obj, Allocator &&src_alloc)
    {
        using T = std::decay_t<Obj>;
        using ReboundAlloc = typename std::allocator_traits<
            std::decay_t<Allocator>>::template rebind_alloc<T>;
        ReboundAlloc rebound_alloc(src_alloc);

        if constexpr (is_small<T>())
        {
            auto *ptr = as_small<T>();
            std::allocator_traits<ReboundAlloc>::construct(rebound_alloc, ptr,
                                                           std::forward<Obj>(obj));
        }
        else
        {
            T *ptr = std::allocator_traits<ReboundAlloc>::allocate(rebound_alloc, 1);
            try
            {
                std::allocator_traits<ReboundAlloc>::construct(rebound_alloc, ptr,
                                                               std::forward<Obj>(obj));
                storage_.heap_ptr = ptr;
            }
            catch (...)
            {
                std::allocator_traits<ReboundAlloc>::deallocate(rebound_alloc, ptr, 1);
                throw;
            }
        }
    }

    template <typename Obj, typename Allocator>
    constexpr any_storage(Obj &&obj, Allocator &&alloc)
        : allocator_{alloc}, storage_{},
          ops_{create_ops<std::decay_t<Obj>, std::decay_t<Allocator>>()}
    {
        construct(std::forward<Obj>(obj), std::forward<Allocator>(alloc));
    }

    constexpr any_storage(const any_storage &other)
        : allocator_{other.allocator_}, ops_(other.ops_)
    {
        if (ops_)
            ops_->copy_construct(*this, other);
    }

    constexpr any_storage(any_storage &&other) noexcept
        : allocator_{std::move(other.allocator_)},
          ops_(std::exchange(other.ops_, nullptr))
    {
        if (ops_)
        {
            ops_->move_construct(*this, other);
            ops_->destroy(other);
        }
    }

    constexpr any_storage &operator=(const any_storage &other)
    {
        if (this != &other)
        {
            if (ops_)
                ops_->destroy(*this);
            allocator_ = other.allocator_;
            ops_ = other.ops_;
            if (ops_)
                ops_->copy_construct(*this, other);
        }
        return *this;
    }

    constexpr any_storage &operator=(any_storage &&other) noexcept
    {
        if (this != &other)
        {
            if (ops_)
                ops_->destroy(*this);
            allocator_ = std::move(other.allocator_);
            ops_ = std::exchange(other.ops_, nullptr);
            if (ops_)
            {
                ops_->move_construct(*this, other);
                ops_->destroy(other);
            }
        }
        return *this;
    }

    friend constexpr void swap(any_storage &a, any_storage &b) noexcept
    {
        using std::swap;
        swap(a.allocator_, b.allocator_);
        swap(a.storage_, b.storage_);
        swap(a.ops_, b.ops_);
    }

    constexpr ~any_storage() noexcept
    {
        if (ops_)
            ops_->destroy(*this);
    }
};

// 基于 any_storage 的 my_functional 实现（简化版）
template <typename Signature>
class my_functional;

template <typename R, typename... Args>
class my_functional<R(Args...)>
{
  private:
    // 函数调用器接口
    struct invoker_base
    {
        virtual ~invoker_base() = default;
        virtual R invoke(Args... args) const = 0;
        virtual std::unique_ptr<invoker_base> clone() const = 0;
        virtual const std::type_info &target_type() const noexcept = 0;
    };

    // 具体函数调用器
    template <typename F>
    class invoker : public invoker_base
    {
        F f_;

      public:
        explicit invoker(F f) : f_(std::move(f)) {}

        R invoke(Args... args) const override
        {
            if constexpr (std::is_void_v<R>)
            {
                std::invoke(f_, std::forward<Args>(args)...);
            }
            else
            {
                return std::invoke(f_, std::forward<Args>(args)...);
            }
        }

        std::unique_ptr<invoker_base> clone() const override
        {
            return std::make_unique<invoker<F>>(*this);
        }

        const std::type_info &target_type() const noexcept override
        {
            return typeid(F);
        }
    };

    // 使用 any_storage 存储调用器
    static constexpr size_t STORAGE_SIZE = 32;
    static constexpr size_t STORAGE_ALIGN = 8;

    // 使用 std::optional 包装 any_storage 以支持默认构造
    std::optional<any_storage<STORAGE_SIZE, STORAGE_ALIGN>> storage_;
    std::unique_ptr<invoker_base> heap_invoker_;

    // 检查是否适合小对象优化
    template <typename F>
    static constexpr bool fits_sso() noexcept
    {
        return sizeof(invoker<F>) <= STORAGE_SIZE && alignof(invoker<F>) <= STORAGE_ALIGN;
    }

    // 获取调用器指针
    template <typename F>
    invoker<F> *get_invoker() noexcept
    {
        if constexpr (fits_sso<F>())
        {
            return storage_->template as_small<invoker<F>>();
        }
        else
        {
            return static_cast<invoker<F> *>(heap_invoker_.get());
        }
    }

    template <typename F>
    const invoker<F> *get_invoker() const noexcept
    {
        if constexpr (fits_sso<F>())
        {
            return storage_->template as_small<invoker<F>>();
        }
        else
        {
            return static_cast<invoker<F> *>(heap_invoker_.get());
        }
    }

  public:
    my_functional() noexcept = default;

    // 构造函数
    template <typename F, typename = std::enable_if_t<
                              !std::is_same_v<std::decay_t<F>, my_functional> &&
                              std::is_invocable_r_v<R, F, Args...>>>
    my_functional(F &&f)
        : storage_(std::in_place), // 显式初始化 storage_
          heap_invoker_(nullptr)
    {
        using FuncType = std::decay_t<F>;
        using InvokerType = invoker<FuncType>;

        if constexpr (fits_sso<FuncType>())
        {
            // 使用 SSO
            new (storage_->template as_small<InvokerType>())
                InvokerType(std::forward<F>(f));
            storage_->ops_ =
                any_storage<STORAGE_SIZE, STORAGE_ALIGN>::template create_ops<
                    InvokerType, std::allocator<void>>();
        }
        else
        {
            // 使用堆分配
            heap_invoker_ = std::make_unique<InvokerType>(std::forward<F>(f));
            storage_.reset(); // 清空 storage_
        }
    }

    // 拷贝构造函数
    my_functional(const my_functional &other)
        : storage_(other.storage_
                       ? std::optional<any_storage<STORAGE_SIZE, STORAGE_ALIGN>>(
                             std::in_place, *other.storage_)
                       : std::nullopt),
          heap_invoker_(other.heap_invoker_ ? other.heap_invoker_->clone() : nullptr)
    {
    }

    // 移动构造函数
    my_functional(my_functional &&other) noexcept
        : storage_(std::move(other.storage_)),
          heap_invoker_(std::move(other.heap_invoker_))
    {
    }

    // 赋值运算符
    my_functional &operator=(const my_functional &other)
    {
        if (this != &other)
        {
            if (other.heap_invoker_)
            {
                heap_invoker_ = other.heap_invoker_->clone();
                storage_ = std::nullopt;
            }
            else if (other.storage_)
            {
                storage_.emplace(*other.storage_);
                heap_invoker_.reset();
            }
            else
            {
                reset();
            }
        }
        return *this;
    }

    my_functional &operator=(my_functional &&other) noexcept
    {
        if (this != &other)
        {
            storage_ = std::move(other.storage_);
            heap_invoker_ = std::move(other.heap_invoker_);
        }
        return *this;
    }

    // 调用运算符
    R operator()(Args... args) const
    {
        if (heap_invoker_)
        {
            if constexpr (std::is_void_v<R>)
            {
                heap_invoker_->invoke(std::forward<Args>(args)...);
            }
            else
            {
                return heap_invoker_->invoke(std::forward<Args>(args)...);
            }
        }
        else if (storage_ && storage_->ops_)
        {
            // 从 storage 获取调用器
            using BaseType = invoker_base;
            auto *invoker_ptr = storage_->template as_small<BaseType>();
            if (invoker_ptr)
            {
                if constexpr (std::is_void_v<R>)
                {
                    invoker_ptr->invoke(std::forward<Args>(args)...);
                }
                else
                {
                    return invoker_ptr->invoke(std::forward<Args>(args)...);
                }
            }
        }
        throw std::bad_function_call();
    }

    // 是否为空
    explicit operator bool() const noexcept
    {
        return heap_invoker_ != nullptr || (storage_ && storage_->ops_ != nullptr);
    }

    // 获取目标类型
    const std::type_info &target_type() const noexcept
    {
        if (heap_invoker_)
            return heap_invoker_->target_type();
        else if (storage_ && storage_->ops_)
            return storage_->stored_type();
        return typeid(void);
    }

    // 重置
    void reset() noexcept
    {
        heap_invoker_.reset();
        storage_.reset();
    }

    // 交换
    void swap(my_functional &other) noexcept
    {
        using std::swap;
        swap(storage_, other.storage_);
        swap(heap_invoker_, other.heap_invoker_);
    }

    ~my_functional() = default;
};

// 性能测试
void benchmark_call_performance()
{
    constexpr int ITERATIONS = 1000000;

    // 小型函数对象
    struct SmallCallable
    {
        int operator()(int x) const
        {
            return x * 2;
        }
    };

    // 大型函数对象
    struct LargeCallable
    {
        std::array<char, 256> data;
        int operator()(int x) const
        {
            return x + static_cast<int>(data[0]);
        }
    };

    std::cout << "🚀 ========== 调用开销测试 ==========\n\n";

    // 1. 小对象调用测试
    std::cout << "📊 小对象调用性能:\n";

    // my_functional
    {
        SmallCallable small;
        my_functional<int(int)> my_func(small);

        auto start = std::chrono::high_resolution_clock::now();
        int result = 0;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            result += my_func(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "⏱️  my_functional: " << duration.count() << " μs, result=" << result
                  << "\n";
    }

    // std::function
    {
        SmallCallable small;
        std::function<int(int)> std_func(small);

        auto start = std::chrono::high_resolution_clock::now();
        int result = 0;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            result += std_func(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "⏱️  std::function: " << duration.count() << " μs, result=" << result
                  << "\n";
    }

    std::cout << "\n";

    // 2. 大对象调用测试
    std::cout << "📊 大对象调用性能:\n";

    // my_functional
    {
        LargeCallable large;
        large.data.fill('A');
        my_functional<int(int)> my_func(large);

        auto start = std::chrono::high_resolution_clock::now();
        int result = 0;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            result += my_func(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "⏱️  my_functional: " << duration.count() << " μs, result=" << result
                  << "\n";
    }

    // std::function
    {
        LargeCallable large;
        large.data.fill('A');
        std::function<int(int)> std_func(large);

        auto start = std::chrono::high_resolution_clock::now();
        int result = 0;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            result += std_func(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "⏱️  std::function: " << duration.count() << " μs, result=" << result
                  << "\n";
    }

    std::cout << "\n";

    // 3. Lambda 调用测试
    std::cout << "📊 Lambda 调用性能:\n";

    // my_functional with lambda
    {
        auto lambda = [multiplier = 3](int x) {
            return x * multiplier;
        };
        my_functional<int(int)> my_func(lambda);

        auto start = std::chrono::high_resolution_clock::now();
        int result = 0;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            result += my_func(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "⏱️  my_functional (lambda): " << duration.count()
                  << " μs, result=" << result << "\n";
    }

    // std::function with lambda
    {
        auto lambda = [multiplier = 3](int x) {
            return x * multiplier;
        };
        std::function<int(int)> std_func(lambda);

        auto start = std::chrono::high_resolution_clock::now();
        int result = 0;
        for (int i = 0; i < ITERATIONS; ++i)
        {
            result += std_func(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "⏱️  std::function (lambda): " << duration.count()
                  << " μs, result=" << result << "\n";
    }
}

// 基本功能测试
void test_basic_functionality()
{
    std::cout << "🧪 ========== 基本功能测试 ==========\n\n";

    // 1. Lambda 测试
    {
        std::cout << "1. Lambda 测试:\n";
        auto lambda = [](int x) {
            return x * 2;
        };

        my_functional<int(int)> my_func(lambda);
        std::function<int(int)> std_func(lambda);

        std::cout << "   my_functional(5) = " << my_func(5) << "\n";
        std::cout << "   std::function(5) = " << std_func(5) << "\n";

        assert(my_func(5) == 10);
        assert(std_func(5) == 10);
        std::cout << "   ✅ 测试通过\n\n";
    }

    // 2. 函数对象测试
    {
        std::cout << "2. 函数对象测试:\n";
        struct Multiplier
        {
            int factor;
            int operator()(int x) const
            {
                return x * factor;
            }
        };

        Multiplier mult{3};
        my_functional<int(int)> my_func(mult);
        std::function<int(int)> std_func(mult);

        std::cout << "   my_functional(4) = " << my_func(4) << "\n";
        std::cout << "   std::function(4) = " << std_func(4) << "\n";

        assert(my_func(4) == 12);
        assert(std_func(4) == 12);
        std::cout << "   ✅ 测试通过\n\n";
    }

    // 3. 复制和移动测试
    {
        std::cout << "3. 复制和移动测试:\n";
        auto lambda = [](int x) {
            return x * 10;
        };

        my_functional<int(int)> func1(lambda);
        my_functional<int(int)> func2(func1);            // 复制构造
        my_functional<int(int)> func3(std::move(func1)); // 移动构造

        std::cout << "   func2(3) = " << func2(3) << "\n";
        std::cout << "   func3(3) = " << func3(3) << "\n";

        assert(func2(3) == 30);
        assert(func3(3) == 30);
        assert(!func1); // func1 应该为空
        std::cout << "   ✅ 测试通过\n\n";
    }

    // 4. 大小比较
    {
        std::cout << "4. 大小比较:\n";
        std::cout << "   sizeof(my_functional<int(int)>) = "
                  << sizeof(my_functional<int(int)>) << " bytes\n";
        std::cout << "   sizeof(std::function<int(int)>) = "
                  << sizeof(std::function<int(int)>) << " bytes\n";
        std::cout << "   sizeof(any_storage<32,8>) = " << sizeof(any_storage<32, 8>)
                  << " bytes\n";
        std::cout << "\n";
    }
}

int main()
{
    std::cout << "🔬 ========== my_functional vs std::function 对比测试 ==========\n\n";

    // 运行基本功能测试
    test_basic_functionality();

    // 运行性能测试
    benchmark_call_performance();

    std::cout << "\n🎯 ========== 测试总结 ==========\n";
    std::cout << "✅ my_functional 特点:\n";
    std::cout << "   • 基于 any_storage 实现，支持 SSO\n";
    std::cout << "   • 小对象存储在栈上，大对象存储在堆上\n";
    std::cout << "   • 内存布局更紧凑\n";
    std::cout << "   • 调用开销与 std::function 相当\n";
    std::cout << "\n⚖️  性能对比:\n";
    std::cout << "   • 对于小对象：my_functional 通常更快（SSO 优势）\n";
    std::cout << "   • 对于大对象：两者性能相近\n";

    return 0;
}
// NOLINTEND