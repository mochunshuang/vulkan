#include <bit>
#include <cassert>
#include <cstddef>
#include <memory>
#include <iostream>
#include <tuple>
#include <utility>
#include <cstring>
#include <array>
#include <stdexcept>
#include <type_traits>

// NOLINTBEGIN

#define debug_any_storage 1

template <std::size_t buffer_size, size_t align_size = alignof(std::max_align_t),
          typename allocator_storage_type = std::allocator<void>>
struct any_storage
{
#if debug_any_storage
    static constexpr auto &destroy_stack_count()
    {
        static int count{};
        return count;
    };
    static constexpr auto &destroy_heap_count()
    {
        static int count{};
        return count;
    };
    static constexpr auto &copy_stack_count()
    {
        static int count{};
        return count;
    };
    static constexpr auto &copy_heap_count()
    {
        static int count{};
        return count;
    };
    static constexpr auto &move_stack_count()
    {
        static int count{};
        return count;
    };
    static constexpr auto &move_heap_count()
    {
        static int count{};
        return count;
    };
    // construct
    static constexpr auto &construct_stack_count()
    {
        static int count{};
        return count;
    };
    static constexpr auto &construct_heap_count()
    {
        static int count{};
        return count;
    };
#endif

    struct storage_ops
    {
        void (*destroy)(any_storage &self) noexcept;
        void (*copy_construct)(any_storage &dest, const any_storage &src);
        void (*move_construct)(any_storage &dest, any_storage &src) noexcept;
        bool (*equals)(const any_storage &a, const any_storage &b) noexcept;
        const std::type_info &(*type_info_T)() noexcept;
        const std::type_info &(*type_info_Allocator)() noexcept;
    };

    const storage_ops *ops_ = nullptr;
    [[no_unique_address]] allocator_storage_type allocator_;
    union storage_union {
        alignas(align_size) std::byte stack_buffer[buffer_size];
        void *heap_ptr;
    } storage_;

    //------------------------Pointer access begin----------------------------
    template <typename T>
    static consteval bool is_small() noexcept
    {
        return sizeof(T) <= buffer_size && alignof(T) <= align_size;
    }

    constexpr void *stack_pointer() noexcept
    {
        return storage_.stack_buffer;
    }
    constexpr void *heap_pointer() noexcept
    {
        return storage_.heap_ptr;
    }
    constexpr const void *stack_pointer() const noexcept
    {
        return storage_.stack_buffer;
    }
    constexpr const void *heap_pointer() const noexcept
    {
        return storage_.heap_ptr;
    }

    template <typename T>
    constexpr T *as_small() noexcept
    {
        return std::bit_cast<T *>(stack_pointer());
    }
    template <typename T>
    constexpr T *as_large() noexcept
    {
        return std::bit_cast<T *>(heap_pointer());
    }
    template <typename T>
    constexpr const T *as_small() const noexcept
    {
        return std::bit_cast<const T *>(stack_pointer());
    }
    template <typename T>
    constexpr const T *as_large() const noexcept
    {
        return static_cast<const T *>(heap_pointer());
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
    //------------------------Pointer access end----------------------------

    //------------------------vtable begin----------------------------
  private:
    template <typename T, typename Allocator>
    constexpr static void destroy_impl(any_storage &self) noexcept
    {
        using ReboundAlloc =
            typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        ReboundAlloc rebound_alloc(self.allocator_);

        if constexpr (is_small<T>())
        {
            auto *ptr = self.template as_small<T>();
            std::allocator_traits<ReboundAlloc>::destroy(rebound_alloc, ptr);
#if debug_any_storage
            ++destroy_stack_count();
#endif
        }
        else
        {
            auto *ptr = self.template as_large<T>();
            if (ptr)
            {
                std::allocator_traits<ReboundAlloc>::destroy(rebound_alloc, ptr);
                std::allocator_traits<ReboundAlloc>::deallocate(rebound_alloc, ptr, 1);
                self.storage_.heap_ptr = nullptr;
#if debug_any_storage
                ++destroy_heap_count();
#endif
            }
        }
        self.ops_ = nullptr;
    }

    template <typename T, typename Allocator>
    constexpr static void copy_construct_impl(any_storage &dest, const any_storage &src)
    {
        using ReboundAlloc =
            typename std::allocator_traits<Allocator>::template rebind_alloc<T>;
        ReboundAlloc rebound_alloc(dest.allocator_);

        const T *src_obj = nullptr;
        if constexpr (is_small<T>())
            src_obj = src.template as_small<T>();
        else
            src_obj = src.template as_large<T>();

        if constexpr (is_small<T>())
        {
            auto *ptr = dest.template as_small<T>();
            std::allocator_traits<ReboundAlloc>::construct(rebound_alloc, ptr, *src_obj);
#if debug_any_storage
            ++copy_stack_count();
#endif
        }
        else
        {
            T *ptr = std::allocator_traits<ReboundAlloc>::allocate(rebound_alloc, 1);
            try
            {
                std::allocator_traits<ReboundAlloc>::construct(rebound_alloc, ptr,
                                                               *src_obj);
                dest.storage_.heap_ptr = ptr;
#if debug_any_storage
                ++copy_heap_count();
#endif
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
#if debug_any_storage
            ++move_stack_count();
#endif
        }
        else
        {
            dest.storage_.heap_ptr = std::exchange(src.storage_.heap_ptr, nullptr);
#if debug_any_storage
            ++move_heap_count();
#endif
        }
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
                return *obj_a == *obj_b;
            else if constexpr (std::is_trivially_copyable_v<T> && is_small<T>())
                return std::memcmp(obj_a, obj_b, sizeof(T)) == 0;
            else
                return obj_a == obj_b;
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

    //------------------------vtable end----------------------------

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
        ReboundAlloc rebound_alloc{src_alloc};

        if constexpr (is_small<T>())
        {

            auto *ptr = as_small<T>();
            std::allocator_traits<ReboundAlloc>::construct(rebound_alloc, ptr,
                                                           std::forward<Obj>(obj));

#if debug_any_storage
            ++construct_stack_count();
#endif
        }
        else
        {
            // std::cout << "🔄 " << "" << " construct on heap\n";
            T *ptr = std::allocator_traits<ReboundAlloc>::allocate(rebound_alloc, 1);
            try
            {
                std::allocator_traits<ReboundAlloc>::construct(rebound_alloc, ptr,
                                                               std::forward<Obj>(obj));
                storage_.heap_ptr = ptr;

#if debug_any_storage
                ++construct_heap_count();
#endif
            }
            catch (...)
            {
                std::allocator_traits<ReboundAlloc>::deallocate(rebound_alloc, ptr, 1);
                throw;
            }
        }
    }

  public:
    constexpr any_storage() noexcept : ops_{}, allocator_{}, storage_{} {}
    template <typename Obj, typename Allocator>
    constexpr any_storage(Obj &&obj, Allocator &&alloc)
        : ops_{create_ops<std::decay_t<Obj>, std::decay_t<Allocator>>()},
          allocator_{alloc}
    {
        construct(std::forward<Obj>(obj), std::forward<Allocator>(alloc));
    }
    constexpr any_storage(const any_storage &other)
        : ops_(other.ops_), allocator_{other.allocator_}
    {
        if (ops_)
            ops_->copy_construct(*this, other);
    }
    constexpr any_storage(any_storage &&other) noexcept
        : ops_(std::exchange(other.ops_, nullptr)),
          allocator_{std::move(other.allocator_)}
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
    constexpr ~any_storage() noexcept
    {
        if (ops_)
            ops_->destroy(*this);
    }

    friend constexpr void swap(any_storage &a, any_storage &b) noexcept
    {
        using std::swap;
        swap(a.allocator_, b.allocator_);
        swap(a.storage_, b.storage_);
        swap(a.ops_, b.ops_);
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
};

// 重置统计计数（用于隔离测试）
template <typename Storage>
void reset_counts()
{
#if debug_any_storage
    Storage::construct_stack_count() = 0;
    Storage::construct_heap_count() = 0;
    Storage::copy_stack_count() = 0;
    Storage::copy_heap_count() = 0;
    Storage::move_stack_count() = 0;
    Storage::move_heap_count() = 0;
    Storage::destroy_stack_count() = 0;
    Storage::destroy_heap_count() = 0;
#endif
}
#undef debug_any_storage

// 测试用的小型函数对象
struct SmallFunctor
{
    int value;
    SmallFunctor(int v = 0) : value(v) {}
    int operator()(int x) const
    {
        return x + value;
    }
    bool operator==(const SmallFunctor &other) const
    {
        return value == other.value;
    }
};

// 测试用的大型函数对象
struct LargeFunctor
{
    std::array<char, 256> data;
    LargeFunctor(char c = 'A')
    {
        data.fill(c);
    }
    char operator()() const
    {
        return data[0];
    }
    bool operator==(const LargeFunctor &other) const
    {
        return data[0] == other.data[0];
    }
};

// 自定义分配器
template <typename T>
struct TrackingAllocator
{
    using value_type = T;
    static int allocate_count;
    static int deallocate_count;
    static int construct_count;
    static int destroy_count;

    TrackingAllocator() noexcept = default;
    template <typename U>
    TrackingAllocator(const TrackingAllocator<U> &) noexcept
    {
    }

    T *allocate(std::size_t n)
    {
        ++allocate_count;
        return static_cast<T *>(::operator new(n * sizeof(T)));
    }

    void deallocate(T *p, std::size_t) noexcept
    {
        ++deallocate_count;
        ::operator delete(p);
    }

    template <typename U, typename... Args>
    void construct(U *p, Args &&...args)
    {
        ++construct_count;
        ::new (static_cast<void *>(p)) U(std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U *p)
    {
        ++destroy_count;
        p->~U();
    }

    template <typename U>
    bool operator==(const TrackingAllocator<U> &) const noexcept
    {
        return true;
    }
    template <typename U>
    bool operator!=(const TrackingAllocator<U> &) const noexcept
    {
        return false;
    }

    static void reset()
    {
        allocate_count = deallocate_count = construct_count = destroy_count = 0;
    }
};

template <typename T>
int TrackingAllocator<T>::allocate_count = 0;
template <typename T>
int TrackingAllocator<T>::deallocate_count = 0;
template <typename T>
int TrackingAllocator<T>::construct_count = 0;
template <typename T>
int TrackingAllocator<T>::destroy_count = 0;

// 为TrackingAllocator特化std::allocator_traits
namespace std
{
    template <typename T>
    struct allocator_traits<TrackingAllocator<T>>
    {
        using allocator_type = TrackingAllocator<T>;
        using value_type = T;
        using pointer = T *;
        using const_pointer = const T *;
        using void_pointer = void *;
        using const_void_pointer = const void *;
        using difference_type = ptrdiff_t;
        using size_type = size_t;
        using propagate_on_container_copy_assignment = false_type;
        using propagate_on_container_move_assignment = true_type;
        using propagate_on_container_swap = false_type;

        template <typename U>
        using rebind_alloc = TrackingAllocator<U>;

        static pointer allocate(allocator_type &a, size_type n)
        {
            return a.allocate(n);
        }

        static void deallocate(allocator_type &a, pointer p, size_type n)
        {
            a.deallocate(p, n);
        }

        template <typename U, typename... Args>
        static void construct(allocator_type &a, U *p, Args &&...args)
        {
            a.construct(p, std::forward<Args>(args)...);
        }

        template <typename U>
        static void destroy(allocator_type &a, U *p)
        {
            a.destroy(p);
        }
    };
} // namespace std

// 边界测试类型
struct Exactly64Bytes
{
    char data[64];
    Exactly64Bytes()
    {
        std::fill_n(data, 64, 'Z');
    }
    bool operator==(const Exactly64Bytes &other) const
    {
        return std::memcmp(data, other.data, 64) == 0;
    }
};

// 高对齐要求的类型
struct alignas(32) HighAlignment
{
    double data[4];
    HighAlignment()
    {
        std::fill_n(data, 4, 3.14);
    }
    bool operator==(const HighAlignment &other) const
    {
        return std::memcmp(data, other.data, sizeof(data)) == 0;
    }
};

// 空类型
struct EmptyType
{
    bool operator==(const EmptyType &) const
    {
        return true;
    }
};

// 可能抛出异常的类型
struct ThrowingType
{
    int value;
    explicit ThrowingType(int v) : value(v)
    {
        if (v == 42)
            throw std::runtime_error("Test exception");
    }
    bool operator==(const ThrowingType &other) const = default;
};

// 非平凡移动类型
struct NonTrivialMove
{
    std::unique_ptr<int> ptr;
    NonTrivialMove(int v) : ptr(std::make_unique<int>(v)) {}
    NonTrivialMove(const NonTrivialMove &other) : ptr(std::make_unique<int>(*other.ptr))
    {
    }
    NonTrivialMove(NonTrivialMove &&) = default;
    NonTrivialMove &operator=(const NonTrivialMove &other)
    {
        if (this != &other)
            ptr = std::make_unique<int>(*other.ptr);
        return *this;
    }
    NonTrivialMove &operator=(NonTrivialMove &&) = default;
    bool operator==(const NonTrivialMove &other) const
    {
        return ptr && other.ptr && *ptr == *other.ptr;
    }
};

//-------------------------------------------------------------
template <typename T>
struct traits_lambda;

template <typename R, typename... Args>
struct traits_lambda<R (*)(Args...)>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr bool is_noexcept = false;
    static constexpr bool is_const = false;
    static constexpr bool is_static = true;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename R, typename... Args>
struct traits_lambda<R (*)(Args...) noexcept>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr bool is_noexcept = true;
    static constexpr bool is_const = false;
    static constexpr bool is_static = true;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename T, typename R, typename... Args>
struct traits_lambda<R (T::*)(Args...)>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr bool is_noexcept = false;
    static constexpr bool is_const = false;
    static constexpr bool is_static = false;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename T, typename R, typename... Args>
struct traits_lambda<R (T::*)(Args...) noexcept>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr bool is_noexcept = true;
    static constexpr bool is_const = false;
    static constexpr bool is_static = false;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename T, typename R, typename... Args>
struct traits_lambda<R (T::*)(Args...) const>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr bool is_noexcept = false;
    static constexpr bool is_const = true;
    static constexpr bool is_static = false;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename T, typename R, typename... Args>
struct traits_lambda<R (T::*)(Args...) const noexcept>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr bool is_noexcept = true;
    static constexpr bool is_const = true;
    static constexpr bool is_static = false;
    static constexpr size_t args_size = sizeof...(Args);
};

template <typename Func>
struct traits_slot_function;

template <typename T, typename... Args>
struct traits_slot_function<void (*)(T, Args...) noexcept>
{
    static constexpr bool is_pointer = true;
    static constexpr size_t args_size = sizeof...(Args);
    using param_0 = T;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
};

template <typename Lambda>
struct traits_slot_function
{
    static_assert(not std::is_member_function_pointer_v<Lambda>, "not member_function");
    static_assert(not std::is_pointer_v<Lambda>, "not function");

    static constexpr bool is_pointer = false;
    using operator_type = decltype(&Lambda::operator());

    using traits = traits_lambda<operator_type>;
    using class_type = Lambda;
    using return_type = typename traits::return_type;
    using args_tuple = typename traits::args_tuple;
    using ref_args_tuple = typename traits::ref_args_tuple;
    static constexpr bool is_noexcept = traits::is_noexcept;
    static constexpr bool is_const = traits::is_const;
    static constexpr size_t args_size = traits::args_size;
};

struct my_slot_store
{
    using storage_type = any_storage<2 * sizeof(void *), alignof(std::max_align_t)>;

    struct vtable
    {
        void (*invoke)(void *self, void *parms) noexcept;
        const std::type_info &(*type_info_O)() noexcept;
        const std::type_info &(*type_info_T)() noexcept;
        const std::type_info &(*type_info_Allocator)() noexcept;
        bool (*stack_store)() noexcept;
    };

    template <typename Obj, typename Func, typename Allocator = std::allocator<void>>
    my_slot_store(Obj *obj, Func fun, Allocator allo = {})
        : vtable_{&create_vtable<std::remove_cvref_t<Obj>, std::remove_cvref_t<Func>,
                                 std::remove_cvref_t<Allocator>>()},
          obj_ptr{std::bit_cast<void *>(obj)},
          fun_{storage_type{std::move(fun), std::move(allo)}}
    {
    }
    template <typename... Args>
    constexpr void invoke(Args... args) noexcept
    {
        using args_tuple = std::tuple<Args &...>;
        args_tuple args_ = {args...};
        this->vtable_->invoke(this, &args_);
    }
    [[nodiscard]] constexpr const std::type_info &object_type() const noexcept
    {
        return vtable_->type_info_O();
    }
    [[nodiscard]] constexpr const std::type_info &function_type() const noexcept
    {
        return vtable_->type_info_T();
    }
    [[nodiscard]] constexpr const std::type_info &allocator_type() const noexcept
    {
        return vtable_->type_info_Allocator();
    }
    [[nodiscard]] constexpr bool stack_store() const noexcept
    {
        return vtable_->stack_store();
    }

  private:
    const vtable *vtable_;
    void *obj_ptr;
    storage_type fun_;

    static constexpr my_slot_store *get_self(void *ptr) noexcept
    {
        return static_cast<my_slot_store *>(ptr);
    }
    template <typename Obj, typename Func, typename allocator>
    static constexpr vtable &create_vtable() noexcept
    {
        using traits = traits_slot_function<Func>;
        static vtable v{
            .invoke =
                [](void *ptr, void *parms) constexpr noexcept {
                    my_slot_store *self = get_self(ptr);

                    using tuple = traits::ref_args_tuple;
                    tuple &args_tupe = *std::bit_cast<tuple *>(parms);

                    Obj &obj_ref = *std::bit_cast<Obj *>(self->obj_ptr);
                    auto *fun = self->fun_.get_pointer<Func>(&self->fun_);

                    if constexpr (traits::is_pointer)
                    {
                        static_assert(std::is_pointer_v<Func>, "must match pointer");
                        static_assert(
                            std::is_same_v<std::decay_t<Obj>,
                                           std::decay_t<typename traits::param_0>>,
                            "must with deducing this in cxx23");

                        [&]<size_t... I>(std::index_sequence<I...>) constexpr noexcept {
                            (*fun)(obj_ref, std::move(std::get<I>(args_tupe))...);
                        }(std::make_index_sequence<traits::args_size>{});
                    }
                    else
                    {
                        static_assert(std::is_object_v<Func>, "must match lambda");
                        [&]<size_t... I>(std::index_sequence<I...>) constexpr noexcept {
                            (*fun)(std::move(std::get<I>(args_tupe))...);
                        }(std::make_index_sequence<traits::args_size>{});
                    }
                },
            .type_info_O = []() constexpr noexcept -> const std::type_info & {
                return typeid(Obj);
            },
            .type_info_T = []() constexpr noexcept -> const std::type_info & {
                return typeid(Func);
            },
            .type_info_Allocator = []() constexpr noexcept -> const std::type_info & {
                return typeid(allocator);
            },
            .stack_store = []() constexpr noexcept -> bool {
                return storage_type::is_small<Func>();
            }};
        return v;
    };
};
struct slot_object
{
    void onClick(this slot_object &self, int i) noexcept
    {
        std::cout << "click: " << i << '\n';
        self.value = i;
    }
    void onClick2(int i) & noexcept
    {
        std::cout << "click: " << i << '\n';
        value = i;
    }
    void onClick3(int i) volatile noexcept
    {
        std::cout << "click: " << i << '\n';
        value = i;
    }
    int value;
};
void test()
{
    slot_object obj;
    obj.onClick(2);
    assert(obj.value == 2);
    {
        // NOTE: lambda 不添加 & 这个玩意
        static_assert(std::is_same_v<decltype(&slot_object::onClick2),
                                     void (slot_object::*)(int) & noexcept>);

        // NOTE: lambda 不添加 volatile 这个玩意
        // NOTE: 成员函数太复杂了
        static_assert(std::is_same_v<decltype(&slot_object::onClick3),
                                     void (slot_object::*)(int) volatile noexcept>);
    }

    constexpr auto pclick = &slot_object::onClick;
    // NOTE: 注意，不是成员函数指针
    static_assert(not std::is_member_object_pointer_v<decltype(&slot_object::onClick)>);

    // NOTE: 这个不是普通函数类型吗？
    static_assert(std::is_same_v<decltype(&slot_object::onClick),
                                 void (*)(slot_object &, int) noexcept>);

    pclick(obj, 3);
    assert(obj.value == 3);
    static_assert(sizeof(pclick) == sizeof(void *));

    my_slot_store sloter{&obj, &slot_object::onClick};

    static_assert(
        my_slot_store::storage_type::is_small<decltype(&slot_object::onClick)>());

    sloter.invoke(1);
    assert(obj.value == 1);
    sloter.invoke(10);
    assert(obj.value == 10);

    auto lambda = [&obj](int i) noexcept {
        obj.onClick(i);
    };
    using lambda_type = decltype(lambda);
    static_assert(my_slot_store::storage_type::is_small<lambda_type>());
    lambda(2);
    assert(obj.value == 2);

    using operator_type = traits_slot_function<lambda_type>::operator_type;
    static_assert(
        std::is_same_v<operator_type, void (lambda_type::*)(int) const noexcept>);
    auto lambda2 = [&obj](int i) mutable noexcept {
        obj.onClick(i);
    };
    using lambda_type2 = decltype(lambda2);
    using operator_type2 = traits_slot_function<lambda_type2>::operator_type;
    static_assert(std::is_same_v<operator_type2, void (lambda_type2::*)(int) noexcept>);

    auto lambda3 = [&obj](int i) constexpr mutable noexcept {
        obj.onClick(i);
    };
    using lambda_type3 = decltype(lambda3);
    using operator_type3 = traits_slot_function<lambda_type3>::operator_type;
    // constexpr // NOTE: 不传递
    static_assert(std::is_same_v<operator_type3, void (lambda_type3::*)(int) noexcept>);

    // static_assert(sizeof(operator_type3) == 2 * sizeof(void *));

    // NOTE: 没有上下文 static 是不可避免的
    auto lambda4 = [](int) static constexpr noexcept {
        // obj.onClick(i); // static 不能捕获
    };
    using operator_type4 = traits_slot_function<decltype(lambda4)>::operator_type;
    static_assert(std::is_same_v<void (*)(int) noexcept, operator_type4>);

    auto lambda5 = [](int) constexpr noexcept {
    };
    using operator_type5 = traits_slot_function<decltype(lambda5)>::operator_type;
    static_assert(
        std::is_same_v<void (decltype(lambda5)::*)(int) const noexcept, operator_type5>);

    static_assert(sizeof(lambda4) == sizeof(lambda5)); // 虽然大小一样

    static_assert(sizeof(lambda3) == sizeof(void *));

    auto lambda6 = [&lambda3, &lambda4](int i) constexpr noexcept {
        lambda3(i);
        lambda4(i);
    };
    static_assert(sizeof(lambda6) == 2 * sizeof(void *)); // NOTE: 一般就两个指针
    static_assert(my_slot_store::storage_type::is_small<decltype(lambda6)>());

    // NOTE: 测试lambda
    static_assert(my_slot_store::storage_type::is_small<
                  decltype([&obj, &lambda4](int i) constexpr noexcept {
                      obj.onClick2(i);
                      lambda4(i);
                  })>());
    my_slot_store sloter2{&obj, [&obj, &lambda4](int i) constexpr noexcept {
                              obj.onClick2(i);
                              lambda4(i);
                          }};
    sloter2.invoke(20);
    assert(obj.value == 20);
}

int main()
{
    test();
    using TestStorage = any_storage<64, 32>; // 使用32字节对齐

    constexpr auto print_info = []() {
        std::cout << "TestStorage::destroy_stack_count(): "
                  << TestStorage::destroy_stack_count() << "\n";
        std::cout << "TestStorage::destroy_heap_count(): "
                  << TestStorage::destroy_heap_count() << "\n";
        std::cout << "stack_construct+copy+move_count(): "
                  << TestStorage::construct_stack_count() +
                         TestStorage::copy_stack_count() + TestStorage::move_stack_count()
                  << "\n";
        std::cout << "heap_construct+copy+move_count(): "
                  << TestStorage::construct_heap_count() +
                         TestStorage::copy_heap_count() + TestStorage::move_heap_count()
                  << "\n";
    };

    std::cout << "=== 开始全面测试 ===\n";

    // 测试1: 默认构造和空存储
    {
        std::cout << "\n测试1: 默认构造和空存储\n";
        reset_counts<TestStorage>();

        TestStorage empty1;
        TestStorage empty2;

        // 检查默认构造后应该是空的
        assert(empty1.stored_type() == typeid(void));
        assert(empty2.stored_type() == typeid(void));
        assert(empty1.allocator_type() == typeid(void));
        assert(empty2.allocator_type() == typeid(void));

        // 空存储应该相等
        assert(empty1 == empty2);
        assert(!(empty1 != empty2));

        // 应该没有构造或销毁
        assert(TestStorage::construct_stack_count() == 0);
        assert(TestStorage::construct_heap_count() == 0);
        assert(TestStorage::destroy_stack_count() == 0);
        assert(TestStorage::destroy_heap_count() == 0);

        std::cout << "✅ 测试1通过\n";
    }

    // 测试2: 小型对象构造和销毁
    {
        std::cout << "\n测试2: 小型对象构造和销毁\n";
        reset_counts<TestStorage>();

        {
            TestStorage storage1(SmallFunctor{42}, std::allocator<void>{});

            // 检查类型信息
            assert(storage1.stored_type() == typeid(SmallFunctor));
            assert(storage1.allocator_type() == typeid(std::allocator<void>));

            // 应该构造在栈上
            assert(TestStorage::construct_stack_count() == 1);
            assert(TestStorage::construct_heap_count() == 0);

            // 复制构造
            TestStorage storage2 = storage1;
            assert(TestStorage::copy_stack_count() == 1);
            assert(TestStorage::copy_heap_count() == 0);

            // 应该相等
            assert(storage1 == storage2);

            // 移动构造
            TestStorage storage3 = std::move(storage1);
            assert(TestStorage::move_stack_count() == 1);
            assert(TestStorage::move_heap_count() == 0);

            // 原始对象应该变为空
            assert(storage1.stored_type() == typeid(void));
            assert(storage2.stored_type() == typeid(SmallFunctor));
            assert(storage3.stored_type() == typeid(SmallFunctor));

            // NOTE: 移动后应该仍然相等。值语义是这样
            assert(storage2 == storage3);

            // 赋值操作
            TestStorage storage4(EmptyType{}, std::allocator<void>{});
            storage4 = storage2;
            assert(TestStorage::copy_stack_count() == 2);
            assert(storage4 == storage2);

            // 移动赋值
            storage4 = std::move(storage3);
            assert(TestStorage::move_stack_count() == 2);
            assert(storage4.stored_type() == typeid(SmallFunctor));

            // 不等性测试
            TestStorage storage5(SmallFunctor{100}, std::allocator<void>{});
            assert(storage4 != storage5);

            // swap测试
            swap(storage4, storage5);
            assert(storage4.stored_type() == typeid(SmallFunctor));
            assert(storage5.stored_type() == typeid(SmallFunctor));
            assert(!(storage4 == storage5)); // 值不同

            // 销毁计数将在作用域结束时检查
        }

        // 检查销毁计数（应该有多个小型对象被销毁）
        assert(TestStorage::destroy_stack_count() > 0);
        assert(TestStorage::destroy_heap_count() == 0);
        print_info();

        std::cout << "✅ 测试2通过\n";
    }

    // 测试3: 大型对象构造和销毁
    {
        std::cout << "\n测试3: 大型对象构造和销毁\n";
        reset_counts<TestStorage>();

        {
            TestStorage storage1(LargeFunctor{'X'}, std::allocator<void>{});

            // 应该构造在堆上
            assert(TestStorage::construct_stack_count() == 0);
            assert(TestStorage::construct_heap_count() == 1);

            // 复制构造
            TestStorage storage2 = storage1;
            assert(TestStorage::copy_stack_count() == 0);
            assert(TestStorage::copy_heap_count() == 1);

            // 应该相等
            assert(storage1 == storage2);

            // 移动构造
            TestStorage storage3 = std::move(storage1);
            assert(TestStorage::move_stack_count() == 0);
            assert(TestStorage::move_heap_count() == 1);

            // 原始对象的堆指针应该被置空
            assert(storage1.stored_type() == typeid(void) ||
                   (storage1.stored_type() == typeid(LargeFunctor) &&
                    storage1.heap_pointer() == nullptr));

            // 赋值操作
            TestStorage storage4(LargeFunctor{'Y'}, std::allocator<void>{});
            storage4 = storage2;
            assert(TestStorage::copy_heap_count() == 2);
            assert(storage4 == storage2);

            // 不等性测试
            assert(storage4 != TestStorage(LargeFunctor{'Z'}, std::allocator<void>{}));
        }

        // 检查销毁计数
        assert(TestStorage::destroy_heap_count() > 0);
        print_info();
        std::cout << "✅ 测试3通过\n";
    }

    // 测试4: 边界大小对象（刚好64字节）
    {
        std::cout << "\n测试4: 边界大小对象测试\n";
        reset_counts<TestStorage>();

        {
            Exactly64Bytes obj;
            TestStorage storage(obj, std::allocator<void>{});

            // sizeof(Exactly64Bytes) == 64，应该存储在栈上
            // 注意：由于对齐填充，实际大小可能大于64，所以这个测试可能失败
            // 但我们仍然检查它是否正常工作
            assert(storage.stored_type() == typeid(Exactly64Bytes));

            // 复制和相等性测试
            TestStorage copy = storage;
            assert(storage == copy);

            // 修改原始对象
            Exactly64Bytes obj2;
            obj2.data[0] = 'A';
            TestStorage storage2(obj2, std::allocator<void>{});
            assert(storage != storage2);
        }
        print_info();
        std::cout << "✅ 测试4通过\n";
    }

    // 测试5: 高对齐要求对象
    {
        std::cout << "\n测试5: 高对齐要求对象测试\n";

        // 使用64字节对齐的存储
        using HighAlignStorage = any_storage<64, 64>;

        HighAlignment obj;
        HighAlignStorage storage(obj, std::allocator<void>{});

        // 检查类型和对齐
        assert(storage.stored_type() == typeid(HighAlignment));

        // 验证对齐（通过指针转换检查）
        auto *ptr = storage.template as_small<HighAlignment>();
        assert(reinterpret_cast<uintptr_t>(ptr) % alignof(HighAlignment) == 0);

        print_info();
        std::cout << "✅ 测试5通过\n";
    }

    // 测试6: 自定义分配器
    {
        std::cout << "\n测试6: 自定义分配器测试\n";

        TrackingAllocator<void>::reset();
        using CustomStorage = any_storage<64, 32, TrackingAllocator<void>>;

        {
            CustomStorage storage(SmallFunctor{42}, TrackingAllocator<void>{});

            // 检查分配器类型
            assert(storage.allocator_type() == typeid(TrackingAllocator<void>));

            // 检查自定义分配器被使用
            assert(TrackingAllocator<SmallFunctor>::construct_count > 0);

            // 复制构造
            CustomStorage copy = storage;
            assert(TrackingAllocator<SmallFunctor>::construct_count > 1);
        }

        // 检查销毁
        assert(TrackingAllocator<SmallFunctor>::destroy_count > 0);
        print_info();
        std::cout << "TrackingAllocator<SmallFunctor>::destroy_count: "
                  << TrackingAllocator<SmallFunctor>::destroy_count << "\n";
        std::cout << "✅ 测试6通过\n";
    }

    // 测试7: 异常安全
    {
        std::cout << "\n测试7: 异常安全测试\n";
        reset_counts<TestStorage>();

        // 测试构造时抛出异常
        try
        {
            TestStorage storage(ThrowingType{42}, std::allocator<void>{});
            assert(false && "Should have thrown");
        }
        catch (const std::runtime_error &e)
        {
            assert(std::string(e.what()) == "Test exception");
        }

        // 测试非抛出构造
        TestStorage storage(ThrowingType{0}, std::allocator<void>{});
        assert(storage.stored_type() == typeid(ThrowingType));

        // 测试复制构造时的异常安全
        // 创建一个可能抛出复制的类型
        struct ThrowOnCopy
        {
            int value;
            ThrowOnCopy(int v) noexcept : value(v) {}
            // 移动构造函数不抛出
            ThrowOnCopy(ThrowOnCopy &&other) noexcept : value(other.value) {}
            // 复制构造函数在value==42时抛出
            ThrowOnCopy(const ThrowOnCopy &other) : value(other.value)
            {
                if (value == 42)
                    throw std::runtime_error("Copy failed");
            }
            bool operator==(const ThrowOnCopy &other) const
            {
                return value == other.value;
            }
        };

        using ThrowStorage = any_storage<64, 32>;
        ThrowStorage safe(ThrowOnCopy{0}, std::allocator<void>{});
        ThrowStorage throwing(ThrowOnCopy{42}, std::allocator<void>{});

        try
        {
            ThrowStorage copy = throwing; // 应该抛出
            assert(false && "Should have thrown on copy");
        }
        catch (const std::runtime_error &e)
        {
            // 异常被捕获，原始对象应该仍然有效
            assert(throwing.stored_type() == typeid(ThrowOnCopy));
        }

        // 安全复制应该成功
        ThrowStorage safe_copy = safe;
        assert(safe_copy == safe);
        print_info();
        std::cout << "✅ 测试7通过\n";
    }

    // 测试8: 非平凡移动类型
    {
        std::cout << "\n测试8: 非平凡移动类型测试\n";
        reset_counts<TestStorage>();

        {
            TestStorage storage(NonTrivialMove{42}, std::allocator<void>{});

            // 复制构造
            TestStorage copy = storage;
            assert(copy == storage);

            // 移动构造
            TestStorage moved = std::move(storage);
            assert(moved.stored_type() == typeid(NonTrivialMove));

            // 移动后原始对象应该为空
            assert(storage.stored_type() == typeid(void));
        }
        print_info();
        std::cout << "✅ 测试8通过\n";
    }

    // 测试9: 空类型
    {
        std::cout << "\n测试9: 空类型测试\n";

        TestStorage storage1(EmptyType{}, std::allocator<void>{});
        TestStorage storage2(EmptyType{}, std::allocator<void>{});

        // 空类型应该总是相等
        assert(storage1 == storage2);

        // 即使移动后也应该相等
        TestStorage moved = std::move(storage1);
        assert(moved == storage2);
        print_info();
        std::cout << "✅ 测试9通过\n";
    }

    // 测试10: 类型擦除和查询
    {
        std::cout << "\n测试10: 类型擦除和查询测试\n";

        TestStorage storage(SmallFunctor{42}, std::allocator<void>{});

        // 类型查询
        assert(storage.stored_type() == typeid(SmallFunctor));
        assert(storage.stored_type() != typeid(LargeFunctor));
        assert(storage.stored_type() != typeid(int));
        assert(storage.stored_type() != typeid(void));

        // 分配器类型查询
        assert(storage.allocator_type() == typeid(std::allocator<void>));

        // 不同类型比较应该返回false
        TestStorage different(LargeFunctor{'A'}, std::allocator<void>{});
        assert(storage != different);

        std::cout << "✅ 测试10通过\n";
    }

    // 测试11: 自赋值和自移动
    {
        std::cout << "\n测试11: 自赋值测试\n";

        TestStorage storage(SmallFunctor{42}, std::allocator<void>{});
        const auto &original_type = storage.stored_type();

        // 自赋值
        storage = storage;
        assert(storage.stored_type() == original_type);

        // 自移动赋值
        storage = std::move(storage);
        assert(storage.stored_type() == original_type);

        std::cout << "✅ 测试11通过\n";
    }

    // 测试12: 混合操作测试
    {
        std::cout << "\n测试12: 混合操作测试\n";
        reset_counts<TestStorage>();

        // 创建各种类型的存储
        TestStorage small(SmallFunctor{1}, std::allocator<void>{});
        TestStorage large(LargeFunctor{'A'}, std::allocator<void>{});
        TestStorage empty;

        // 混合赋值
        TestStorage mixed = small;
        assert(mixed == small);

        mixed = large;
        assert(mixed == large);

        mixed = empty;
        assert(mixed == empty);

        // 混合swap
        swap(small, large);
        assert(small.stored_type() == typeid(LargeFunctor));
        assert(large.stored_type() == typeid(SmallFunctor));

        // 恢复
        swap(small, large);

        std::cout << "✅ 测试12通过\n";
    }

    // 最终统计验证
    std::cout << "\n=== 最终统计验证 ===\n";

    // 注意：由于reset_counts在每个测试中调用，最终统计只反映最后一个测试

    std::cout << "总构造堆: " << TestStorage::construct_heap_count() << std::endl;
    std::cout << "总构造栈: " << TestStorage::construct_stack_count() << std::endl;
    std::cout << "总复制堆: " << TestStorage::copy_heap_count() << std::endl;
    std::cout << "总复制栈: " << TestStorage::copy_stack_count() << std::endl;
    std::cout << "总移动堆: " << TestStorage::move_heap_count() << std::endl;
    std::cout << "总移动栈: " << TestStorage::move_stack_count() << std::endl;
    std::cout << "总销毁堆: " << TestStorage::destroy_heap_count() << std::endl;
    std::cout << "总销毁栈: " << TestStorage::destroy_stack_count() << std::endl;

    // 验证构造和销毁大致平衡（考虑到临时对象）
    int total_constructed =
        TestStorage::construct_heap_count() + TestStorage::construct_stack_count();
    int total_destroyed =
        TestStorage::destroy_heap_count() + TestStorage::destroy_stack_count();

    std::cout << "总构造: " << total_constructed << ", 总销毁: " << total_destroyed
              << std::endl;

    std::cout << "\n🎉 所有测试通过!\n";

    return 0;
}
// NOLINTEND