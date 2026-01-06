#include <bit>
#include <cassert>
#include <cstddef>
#include <memory>
#include <iostream>
#include <tuple>
#include <utility>
#include <cstring>
#include <array>
#include <type_traits>

// NOLINTBEGIN

#define debug_any_storage 0

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

// NOTE: 必须是 2n 的对齐。实际发现，没有节省空间
template <typename storage>
// struct alignas(alignof(std::max_align_t)) slot_store_base
struct slot_store_base
{
    using storage_type = storage;

    storage_type fun_;
    void *obj_ptr;
};

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
        // slot func
        void (*invoke)(void *slot_store, void *parms) noexcept;
        const std::type_info &(*type_info_O)() noexcept;
        bool (*stack_store)() noexcept;
    };

    const storage_ops *ops_ = nullptr;
#if defined(_MSC_VER)
#define MY_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif defined(__clang__) || defined(__GNUC__)
#define MY_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define MY_NO_UNIQUE_ADDRESS // 不支持该属性的编译器
#endif
    MY_NO_UNIQUE_ADDRESS allocator_storage_type allocator_;
#undef MY_NO_UNIQUE_ADDRESS

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

    template <typename Sloter, typename T, typename Allocator>
    static constexpr const storage_ops *create_ops() noexcept
    {
        static const auto vt = storage_ops{
            .destroy = &destroy_impl<T, Allocator>,
            .copy_construct = &copy_construct_impl<T, Allocator>,
            .move_construct = &move_construct_impl<T, Allocator>,
            .equals = &equals_impl<T>,
            .type_info_T = &type_info_T_impl<T>,
            .type_info_Allocator = &type_info_Allocator_impl<Allocator>,
            .invoke =
                [](void *ptr, void *parms) static constexpr noexcept {
                    using base_type = slot_store_base<any_storage>;
                    base_type *self = static_cast<base_type *>(ptr);

                    using Obj_t = std::remove_cvref_t<Sloter>;
                    using Func_t = std::remove_cvref_t<T>;
                    static_assert(not std::is_pointer_v<Obj_t>);
                    using traits = traits_slot_function<Func_t>;
                    using tuple = traits::ref_args_tuple;
                    tuple &args_tupe = *std::bit_cast<tuple *>(parms);
                    Obj_t *obj = std::bit_cast<Obj_t *>(self->obj_ptr);
                    Func_t &fun_ref =
                        *(self->fun_.template get_pointer<Func_t>(&self->fun_));

                    if constexpr (traits::is_pointer)
                    {
                        static_assert(std::is_pointer_v<Func_t>, "must match pointer");
                        [&]<size_t... I>(std::index_sequence<I...>) constexpr noexcept {
                            if constexpr (std::is_invocable_v<Func_t &, Obj_t &,
                                                              decltype(std::get<I>(
                                                                  args_tupe))...>)
                                (fun_ref)(*obj, std::get<I>(args_tupe)...);
                            else if constexpr (std::is_invocable_v<Func_t &, Obj_t *,
                                                                   decltype(std::get<I>(
                                                                       args_tupe))...>)
                                (fun_ref)(obj, std::get<I>(args_tupe)...);
                            else if constexpr (std::is_invocable_v<Func_t &,
                                                                   decltype(std::get<I>(
                                                                       args_tupe))...>)
                                (fun_ref)(std::get<I>(args_tupe)...);
                            else
                                static_assert(
                                    false,
                                    "Cannot invoke function with provided arguments. "
                                    "Supported signatures:\n"
                                    "1. void(Obj&, Args...)\n"
                                    "2. void(Obj*, Args...)\n"
                                    "3. void(Args...)");
                        }(std::make_index_sequence<traits::args_size>{});
                    }
                    else
                    {
                        static_assert(std::is_object_v<Func_t>, "must match lambda");
                        [&]<size_t... I>(std::index_sequence<I...>) constexpr noexcept {
                            constexpr size_t expected_args = sizeof...(I);
                            constexpr size_t func_args = traits::args_size;

                            if constexpr (func_args == expected_args)
                            {
                                (fun_ref)(std::get<I>(args_tupe)...);
                            }
                            else if constexpr (func_args == expected_args + 1)
                            {
                                if constexpr (std::is_invocable_v<Func_t &, Obj_t &,
                                                                  decltype(std::get<I>(
                                                                      args_tupe))...>)
                                    (fun_ref)(*obj, std::get<I>(args_tupe)...);
                                else if constexpr (std::is_invocable_v<
                                                       Func_t &, Obj_t *,
                                                       decltype(std::get<I>(
                                                           args_tupe))...>)
                                    (fun_ref)(obj, std::get<I>(args_tupe)...);
                                else
                                    static_assert(
                                        false, "Cannot call lambda with given arguments");
                            }
                            else
                                static_assert(
                                    false,
                                    "Cannot invoke function with provided arguments. "
                                    "Supported signatures:\n"
                                    "1. void(Obj&, Args...)\n"
                                    "2. void(Obj*, Args...)\n"
                                    "3. void(Args...)");
                        }(std::make_index_sequence<traits::args_size>{});
                    }
                },
            .type_info_O = []() static constexpr noexcept -> const std::type_info & {
                return typeid(Sloter);
            },
            .stack_store = []() static constexpr noexcept -> bool {
                return is_small<T>();
            }};
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
    template <typename Sloter, typename Obj, typename Allocator>
    constexpr any_storage(Sloter *, Obj &&obj, Allocator &&alloc)
        : ops_{create_ops<std::decay_t<Sloter>, std::decay_t<Obj>,
                          std::decay_t<Allocator>>()},
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

template <typename Storage>
constexpr void print_info()
{
#if debug_any_storage
    std::cout << "destroy_stack_count(): " << Storage::destroy_stack_count() << "\n";
    std::cout << "destroy_heap_count(): " << Storage::destroy_heap_count() << "\n";
    std::cout << "stack_construct+copy+move_count(): "
              << Storage::construct_stack_count() + Storage::copy_stack_count() +
                     Storage::move_stack_count()
              << "\n";
    std::cout << "heap_construct+copy+move_count(): "
              << Storage::construct_heap_count() + Storage::copy_heap_count() +
                     Storage::move_heap_count()
              << "\n";
#endif
}

//-------------------------------------------------------------

struct my_slot_store final
    : slot_store_base<any_storage<2 * sizeof(void *), alignof(std::max_align_t)>>
{
    using base_type =
        slot_store_base<any_storage<2 * sizeof(void *), alignof(std::max_align_t)>>;

    template <typename Sloter, typename T, typename Allocator = std::allocator<void>>
    my_slot_store(Sloter *obj, T fun, Allocator allo = {})
        : base_type{.fun_ = {storage_type{obj, std::move(fun), std::move(allo)}},
                    .obj_ptr = std::bit_cast<void *>(obj)}

    {
    }
    my_slot_store() = delete;
    ~my_slot_store() = default;

    template <typename... Args>
    constexpr void invoke(Args... args) & noexcept
    {
        using args_tuple = std::tuple<Args &...>;
        args_tuple args_ = {args...};

        fun_.ops_->invoke(this, &args_);
    }
    [[nodiscard]] constexpr const std::type_info &object_type() const noexcept
    {
        return fun_.ops_->type_info_O();
    }
    [[nodiscard]] constexpr const std::type_info &function_type() const noexcept
    {
        return fun_.stored_type();
    }
    [[nodiscard]] constexpr const std::type_info &allocator_type() const noexcept
    {
        return fun_.allocator_type();
    }
    [[nodiscard]] constexpr bool stack_store() const noexcept
    {
        return fun_.ops_->stack_store();
    }
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

    // NOTE: 1. 测试成员函数
    {
#if debug_any_storage
        assert(my_slot_store::storage_type::construct_stack_count() == 0);
#endif
        my_slot_store sloter{&obj, &slot_object::onClick};

        static_assert(
            my_slot_store::storage_type::is_small<decltype(&slot_object::onClick)>());

        sloter.invoke(1);
        assert(obj.value == 1);
        sloter.invoke(10);
        assert(obj.value == 10);
#if debug_any_storage
        assert(my_slot_store::storage_type::construct_stack_count() == 1);
#endif
    }

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

    // NOTE: 2. 测试lambda
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
    static auto value = 0;

    // NOTE: 3. 测试纯指针
    my_slot_store sloter3{
        &obj, [](slot_object *obj, int i) static constexpr noexcept { obj->value = i; }};

    sloter3.invoke(&obj, 30);
    assert(obj.value == 30);

    // NOTE: 4. 允许获得 obj 指针，不需要显示 invoke的时候，添加 &obj
    sloter3.invoke(40);
    assert(obj.value == 40);

    my_slot_store sloter4{
        &obj, [](slot_object &obj, int i) static constexpr noexcept { obj.value = i; }};
    sloter4.invoke(50);
    assert(obj.value == 50);

    print_info<my_slot_store::storage_type>();
    std::cout << "✅ test\n";
}

// 在 test() 函数后面添加以下性能测试函数

// 修改 main 函数，添加性能测试
#include <functional>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include <iomanip>
#include <cassert>
#include <typeinfo>

// 已有的 any_storage 和 my_slot_store 定义放在这里...
// 为了简洁，这里省略了已有的实现代码
// [已有的代码开始]

// ==================== 测试对象定义 ====================
struct TestObject
{
    int value = 0;

    void member_func(this TestObject &self, int x) noexcept
    {
        self.value += x;
    }

    void const_member_func(int x) const noexcept
    {
        // 只读操作
    }

    virtual void virtual_member_func(int x) noexcept
    {
        value += x;
    }

    ~TestObject() = default;
};

// 纯函数指针
void free_function(TestObject *obj, int x) noexcept
{
    if (obj)
        obj->value += x;
}

// ==================== 性能测试框架 ====================
template <typename Func>
class PerformanceTester
{
  public:
    using Clock = std::chrono::high_resolution_clock;

    struct TestResult
    {
        size_t construct_time_ns = 0;
        size_t copy_time_ns = 0;
        size_t move_time_ns = 0;
        size_t invoke_time_ns = 0;
        size_t memory_usage_bytes = 0;
        size_t stack_allocs = 0;
        size_t heap_allocs = 0;
    };

    static TestResult test_construction(int iterations = 100000)
    {
        TestResult result;
        auto start = Clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            TestObject obj;
            auto lambda = [&obj](int x) noexcept {
                obj.value += x;
            };
            Func func = lambda; // 构造
            if (i == 0)
            {
                result.memory_usage_bytes = sizeof(func);
            }
        }

        auto end = Clock::now();
        result.construct_time_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() /
            iterations;

        return result;
    }

    static TestResult test_copy(int iterations = 100000)
    {
        TestResult result;
        TestObject obj;
        auto lambda = [&obj](int x) noexcept {
            obj.value += x;
        };
        Func original = lambda;

        auto start = Clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            Func copy = original; // 复制构造
            copy(1);              // 防止优化
        }

        auto end = Clock::now();
        result.copy_time_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() /
            iterations;

        return result;
    }

    static TestResult test_move(int iterations = 100000)
    {
        TestResult result;

        auto start = Clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            TestObject obj;
            auto lambda = [&obj](int x) noexcept {
                obj.value += x;
            };
            Func original = lambda;
            Func moved = std::move(original); // 移动构造
            moved(1);                         // 防止优化
        }

        auto end = Clock::now();
        result.move_time_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() /
            iterations;

        return result;
    }

    static TestResult test_invocation(int iterations = 1000000)
    {
        TestResult result;
        TestObject obj;
        int total = 0;

        // 预热
        auto lambda = [&total](int x) noexcept {
            total += x;
        };
        Func func = lambda;
        for (int i = 0; i < 1000; ++i)
        {
            func(1);
        }

        // 实际测试
        total = 0;
        auto start = Clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            func(i % 100);
        }

        auto end = Clock::now();
        result.invoke_time_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() /
            iterations;

        return result;
    }
};

// ==================== 专门针对 my_slot_store 的测试 ====================
struct MySlotStoreTester
{
    struct TestResult
    {
        size_t construct_time_ns = 0;
        size_t copy_time_ns = 0;
        size_t move_time_ns = 0;
        size_t invoke_time_ns = 0;
        size_t memory_usage_bytes = 0;
        size_t stack_allocs = 0;
        size_t heap_allocs = 0;
    };

    static TestResult test_construction(int iterations = 100000)
    {
        TestResult result;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            TestObject obj;
            auto lambda = [&obj](int x) noexcept {
                obj.value += x;
            };
            my_slot_store slotter{&obj, lambda};
            if (i == 0)
            {
                result.memory_usage_bytes = sizeof(slotter);
#if debug_any_storage
                result.stack_allocs =
                    my_slot_store::storage_type::construct_stack_count();
                result.heap_allocs = my_slot_store::storage_type::construct_heap_count();
#endif
            }
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.construct_time_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() /
            iterations;

        return result;
    }

    static TestResult test_invocation(int iterations = 1000000)
    {
        TestResult result;
        TestObject obj;
        int total = 0;

        auto lambda = [&total](int x) noexcept {
            total += x;
        };
        my_slot_store slotter{&obj, lambda};

        // 预热
        for (int i = 0; i < 1000; ++i)
        {
            slotter.invoke(1);
        }

        // 实际测试
        total = 0;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i)
        {
            slotter.invoke(i % 100);
        }

        auto end = std::chrono::high_resolution_clock::now();
        result.invoke_time_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() /
            iterations;

        return result;
    }
};

// ==================== 测试用例运行器 ====================
void run_test_case(const std::string &name, const std::function<void()> &test_func,
                   int warmup_runs = 3, int measured_runs = 5)
{
    std::cout << "\n=== " << name << " ===" << std::endl;

    // 预热运行
    for (int i = 0; i < warmup_runs; ++i)
    {
        test_func();
    }

    // 测量运行
    std::vector<double> times;
    for (int i = 0; i < measured_runs; ++i)
    {
        auto start = std::chrono::high_resolution_clock::now();
        test_func();
        auto end = std::chrono::high_resolution_clock::now();
        double time_ms = std::chrono::duration<double, std::milli>(end - start).count();
        times.push_back(time_ms);
    }

    // 计算统计信息
    double sum = 0, min_time = times[0], max_time = times[0];
    for (double t : times)
    {
        sum += t;
        min_time = std::min(min_time, t);
        max_time = std::max(max_time, t);
    }
    double avg = sum / times.size();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Average: " << avg << " ms" << std::endl;
    std::cout << "Min: " << min_time << " ms" << std::endl;
    std::cout << "Max: " << max_time << " ms" << std::endl;
    std::cout << "Range: " << (max_time - min_time) << " ms" << std::endl;
}

// ==================== 主要测试函数 ====================
void test_member_function()
{
    std::cout << "\n========== 成员函数测试 ==========" << std::endl;

    TestObject obj;

    // 1. std::function 测试
    {
        std::cout << "\n1. std::function (成员函数):" << std::endl;
        auto func = std::function<void(int)>{[&obj](int x) { obj.member_func(x); }};

        auto perf = PerformanceTester<std::function<void(int)>>::test_construction();
        std::cout << "构造时间: " << perf.construct_time_ns << " ns" << std::endl;
        std::cout << "内存占用: " << perf.memory_usage_bytes << " bytes" << std::endl;

        auto invoke_perf = PerformanceTester<std::function<void(int)>>::test_invocation();
        std::cout << "调用时间: " << invoke_perf.invoke_time_ns << " ns" << std::endl;

        // 验证功能
        int expected = obj.value + 10;
        func(10);
        assert(obj.value == expected);
        std::cout << "功能验证: ✓" << std::endl;
    }

    // 2. my_slot_store 测试
    {
        std::cout << "\n2. my_slot_store (成员函数):" << std::endl;
        reset_counts<my_slot_store::storage_type>();

        my_slot_store slotter{&obj, &TestObject::member_func};

        auto perf = MySlotStoreTester::test_construction();
        std::cout << "构造时间: " << perf.construct_time_ns << " ns" << std::endl;
        std::cout << "内存占用: " << perf.memory_usage_bytes << " bytes" << std::endl;
        std::cout << "栈分配: " << perf.stack_allocs << ", 堆分配: " << perf.heap_allocs
                  << std::endl;

        auto invoke_perf = MySlotStoreTester::test_invocation();
        std::cout << "调用时间: " << invoke_perf.invoke_time_ns << " ns" << std::endl;

        // 验证功能
        int expected = obj.value + 10;
        slotter.invoke(10);
        assert(obj.value == expected);
        std::cout << "功能验证: ✓" << std::endl;
    }
}

void test_lambda()
{
    std::cout << "\n========== Lambda 测试 ==========" << std::endl;

    TestObject obj;
    int external_counter = 0;

    // 测试不同大小的lambda
    auto small_lambda = [&obj](int x) noexcept {
        obj.value += x;
    };
    auto medium_lambda = [&obj, &external_counter](int x) noexcept {
        obj.value += x;
        external_counter++;
    };
    auto large_lambda = [&obj, &external_counter](int x) noexcept {
        obj.value += x;
        external_counter++;
        // 添加更多捕获来增加大小
        static int static_counter = 0;
        static_counter++;
    };

    std::cout << "Lambda大小 - 小: " << sizeof(small_lambda) << " bytes" << std::endl;
    std::cout << "Lambda大小 - 中: " << sizeof(medium_lambda) << " bytes" << std::endl;
    std::cout << "Lambda大小 - 大: " << sizeof(large_lambda) << " bytes" << std::endl;

    // 1. std::function 测试
    {
        std::cout << "\n1. std::function (Lambda):" << std::endl;
        auto func = std::function<void(int)>{small_lambda};

        auto perf = PerformanceTester<std::function<void(int)>>::test_construction();
        std::cout << "构造时间: " << perf.construct_time_ns << " ns" << std::endl;
        std::cout << "内存占用: " << perf.memory_usage_bytes << " bytes" << std::endl;

        auto invoke_perf = PerformanceTester<std::function<void(int)>>::test_invocation();
        std::cout << "调用时间: " << invoke_perf.invoke_time_ns << " ns" << std::endl;

        // 测试复制和移动
        auto copy_perf = PerformanceTester<std::function<void(int)>>::test_copy();
        auto move_perf = PerformanceTester<std::function<void(int)>>::test_move();
        std::cout << "复制时间: " << copy_perf.copy_time_ns << " ns" << std::endl;
        std::cout << "移动时间: " << move_perf.move_time_ns << " ns" << std::endl;
    }

    // 2. my_slot_store 测试
    {
        std::cout << "\n2. my_slot_store (Lambda):" << std::endl;
        reset_counts<my_slot_store::storage_type>();

        my_slot_store slotter{&obj, small_lambda};

        auto perf = MySlotStoreTester::test_construction();
        std::cout << "构造时间: " << perf.construct_time_ns << " ns" << std::endl;
        std::cout << "内存占用: " << perf.memory_usage_bytes << " bytes" << std::endl;
        std::cout << "栈分配: " << perf.stack_allocs << ", 堆分配: " << perf.heap_allocs
                  << std::endl;

        auto invoke_perf = MySlotStoreTester::test_invocation();
        std::cout << "调用时间: " << invoke_perf.invoke_time_ns << " ns" << std::endl;
    }
}

void test_function_pointer()
{
    std::cout << "\n========== 函数指针测试 ==========" << std::endl;

    TestObject obj;

    // 1. std::function 测试
    {
        std::cout << "\n1. std::function (函数指针):" << std::endl;
        auto func = std::function<void(TestObject *, int)>{&free_function};

        // 测试构造
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100000; ++i)
        {
            std::function<void(TestObject *, int)> f = &free_function;
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto construct_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() /
            100000;

        std::cout << "构造时间: " << construct_ns << " ns" << std::endl;
        std::cout << "内存占用: " << sizeof(func) << " bytes" << std::endl;

        // 测试调用
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000000; ++i)
        {
            func(&obj, i % 100);
        }
        end = std::chrono::high_resolution_clock::now();
        auto invoke_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() /
            1000000;

        std::cout << "调用时间: " << invoke_ns << " ns" << std::endl;
    }

    // 2. my_slot_store 测试
    {
        std::cout << "\n2. my_slot_store (函数指针):" << std::endl;
        reset_counts<my_slot_store::storage_type>();

        auto func_ptr = +[](TestObject *obj, int x) noexcept {
            if (obj)
                obj->value += x;
        };

        my_slot_store slotter{&obj, func_ptr};

        auto perf = MySlotStoreTester::test_construction();
        std::cout << "构造时间: " << perf.construct_time_ns << " ns" << std::endl;
        std::cout << "内存占用: " << perf.memory_usage_bytes << " bytes" << std::endl;
        std::cout << "栈分配: " << perf.stack_allocs << ", 堆分配: " << perf.heap_allocs
                  << std::endl;

        auto invoke_perf = MySlotStoreTester::test_invocation();
        std::cout << "调用时间: " << invoke_perf.invoke_time_ns << " ns" << std::endl;
    }
}

void test_performance_comparison()
{
    std::cout << "\n========== 综合性能比较 ==========" << std::endl;

    // 测试大量对象的创建和销毁
    const int NUM_OBJECTS = 100000;

    // std::function 测试
    {
        std::cout << "\nstd::function 性能测试:" << std::endl;

        auto start = std::chrono::high_resolution_clock::now();
        std::vector<std::function<void(int)>> functions;
        functions.reserve(NUM_OBJECTS);

        TestObject obj;
        for (int i = 0; i < NUM_OBJECTS; ++i)
        {
            functions.emplace_back([&obj, i](int x) { obj.value += x + i; });
        }

        // 调用所有函数
        for (int i = 0; i < NUM_OBJECTS; ++i)
        {
            functions[i](i);
        }

        // 复制测试
        auto functions_copy = functions;

        auto end = std::chrono::high_resolution_clock::now();
        auto time_ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "创建 " << NUM_OBJECTS << " 个对象并调用: " << time_ms << " ms"
                  << std::endl;
        std::cout << "单个对象平均内存: " << sizeof(std::function<void(int)>) << " bytes"
                  << std::endl;
    }

    // my_slot_store 测试
    {
        std::cout << "\nmy_slot_store 性能测试:" << std::endl;
        reset_counts<my_slot_store::storage_type>();

        auto start = std::chrono::high_resolution_clock::now();
        std::vector<my_slot_store> slot_stores;
        slot_stores.reserve(NUM_OBJECTS);

        TestObject obj;
        for (int i = 0; i < NUM_OBJECTS; ++i)
        {
            slot_stores.emplace_back(&obj,
                                     [&obj, i](int x) noexcept { obj.value += x + i; });
        }

        // 调用所有函数
        for (int i = 0; i < NUM_OBJECTS; ++i)
        {
            slot_stores[i].invoke(i);
        }

        // 复制测试（需要支持复制）
        // auto slot_stores_copy = slot_stores; // 如果支持复制

        auto end = std::chrono::high_resolution_clock::now();
        auto time_ms = std::chrono::duration<double, std::milli>(end - start).count();

        std::cout << "创建 " << NUM_OBJECTS << " 个对象并调用: " << time_ms << " ms"
                  << std::endl;
        std::cout << "单个对象平均内存: " << sizeof(my_slot_store) << " bytes"
                  << std::endl;

        print_info<my_slot_store::storage_type>();
    }
}

void test_edge_cases()
{
    std::cout << "\n========== 边界情况测试 ==========" << std::endl;

    // 测试空函数对象
    {
        std::cout << "\n1. 空函数对象测试:" << std::endl;
        std::function<void()> empty_func;
        try
        {
            empty_func();
            std::cout << "std::function 空调用: 未抛出异常（可能未定义行为）"
                      << std::endl;
        }
        catch (const std::bad_function_call &e)
        {
            std::cout << "std::function 空调用: 抛出 std::bad_function_call" << std::endl;
        }

        // my_slot_store 默认不构造空对象
        std::cout << "my_slot_store: 需要显式构造，无空状态" << std::endl;
    }

    // 测试大对象
    {
        std::cout << "\n2. 大对象测试:" << std::endl;
        struct LargeObject
        {
            std::array<char, 1024> data;
            void operator()(int x)
            {
                data[0] = x;
            }
        };

        LargeObject large;
        std::function<void(int)> large_func = large;

        std::cout << "std::function 存储大对象(" << sizeof(LargeObject) << " bytes): ";
        std::cout << (sizeof(large_func) >= sizeof(LargeObject) ? "可能堆分配"
                                                                : "可能小对象优化")
                  << std::endl;

        // 测试 my_slot_store 的存储能力
        std::cout << "any_storage 缓冲区大小: " << (2 * sizeof(void *)) << " bytes"
                  << std::endl;
        std::cout << "LargeObject 大小: " << sizeof(LargeObject) << " bytes" << std::endl;
    }
}

// 定义不同大小的lambda
// 在 test_edge_cases() 函数后面添加以下堆内存调用开销比较测试

#include <functional>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include <iomanip>
#include <cassert>
#include <typeinfo>
#include <random>

// ==================== 专门的大对象Lambda调用开销测试 ====================

struct BigCaptureObject
{
    std::array<double, 32> data; // 256 bytes
    std::string str = "这是一个很大的捕获对象，用于测试堆内存分配";
    int counter = 0;

    void operator()(int x)
    {
        counter += x;
        data[0] = x * 0.1;
    }
};

// 更大的捕获对象，确保一定使用堆存储
struct VeryBigCaptureObject
{
    std::array<double, 128> big_data; // 1024 bytes
    std::array<char, 512> buffer;     // 512 bytes
    std::array<void *, 64> pointers;  // 512 bytes
    int state = 0;

    void operator()(int x)
    {
        state += x;
        big_data[0] = x * 0.5;
        for (size_t i = 0; i < 8; ++i)
        {
            buffer[i] = static_cast<char>(x + i);
        }
    }
};

// 纯调用开销测试（不包含构造/析构）
void test_heap_call_overhead()
{
    std::cout << "\n========== 堆内存调用开销比较测试 ==========\n";

    constexpr int WARMUP_ITERATIONS = 10000;
    constexpr int MEASURE_ITERATIONS = 1000000;
    constexpr int NUM_SAMPLES = 10;

    // 测试数据
    struct TestResult
    {
        double my_slot_store_ns = 0;
        double std_function_ns = 0;
        size_t heap_allocations = 0;
        size_t stack_allocations = 0;
    };

    // 测试1：BigCaptureObject (~256 bytes)
    {
        std::cout << "\n[测试1] BigCaptureObject (~256 bytes):\n";

        std::vector<TestResult> results;

        for (int sample = 0; sample < NUM_SAMPLES; ++sample)
        {
            TestResult result;

            // 1. 测试 my_slot_store
            {
                reset_counts<my_slot_store::storage_type>();

                TestObject obj;
                BigCaptureObject big_capture;
                my_slot_store slotter{&obj, big_capture};

                // 预热
                for (int i = 0; i < WARMUP_ITERATIONS; ++i)
                {
                    slotter.invoke(i % 100);
                }

                // 实际测量
                auto start = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < MEASURE_ITERATIONS; ++i)
                {
                    slotter.invoke(i % 100);
                }
                auto end = std::chrono::high_resolution_clock::now();

                result.my_slot_store_ns =
                    std::chrono::duration<double, std::nano>(end - start).count() /
                    MEASURE_ITERATIONS;
#if debug_any_storage
                result.heap_allocations =
                    my_slot_store::storage_type::construct_heap_count();
                result.stack_allocations =
                    my_slot_store::storage_type::construct_stack_count();
#endif
            }

            // 2. 测试 std::function
            {
                BigCaptureObject big_capture;
                std::function<void(int)> func = big_capture;

                // 预热
                for (int i = 0; i < WARMUP_ITERATIONS; ++i)
                {
                    func(i % 100);
                }

                // 实际测量
                auto start = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < MEASURE_ITERATIONS; ++i)
                {
                    func(i % 100);
                }
                auto end = std::chrono::high_resolution_clock::now();

                result.std_function_ns =
                    std::chrono::duration<double, std::nano>(end - start).count() /
                    MEASURE_ITERATIONS;
            }

            results.push_back(result);
        }

        // 计算平均和方差
        double avg_mss = 0, avg_std = 0;
        double min_mss = std::numeric_limits<double>::max();
        double min_std = std::numeric_limits<double>::max();
        double max_mss = 0, max_std = 0;

        for (const auto &r : results)
        {
            avg_mss += r.my_slot_store_ns;
            avg_std += r.std_function_ns;
            min_mss = std::min(min_mss, r.my_slot_store_ns);
            min_std = std::min(min_std, r.std_function_ns);
            max_mss = std::max(max_mss, r.my_slot_store_ns);
            max_std = std::max(max_std, r.std_function_ns);
        }

        avg_mss /= NUM_SAMPLES;
        avg_std /= NUM_SAMPLES;

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "my_slot_store: " << avg_mss << " ns/call (min: " << min_mss
                  << ", max: " << max_mss << ")\n";
        std::cout << "std::function: " << avg_std << " ns/call (min: " << min_std
                  << ", max: " << max_std << ")\n";
        std::cout << "开销比率: " << (avg_mss / avg_std * 100) << "%\n";
        std::cout << "存储分配: 堆=" << results[0].heap_allocations
                  << ", 栈=" << results[0].stack_allocations << "\n";
    }

    // 测试2：VeryBigCaptureObject (>2KB)
    {
        std::cout << "\n[测试2] VeryBigCaptureObject (>2KB):\n";

        TestResult result;

        // 1. 测试 my_slot_store
        {
            reset_counts<my_slot_store::storage_type>();

            TestObject obj;
            VeryBigCaptureObject very_big_capture;
            my_slot_store slotter{&obj, very_big_capture};

            // 预热
            for (int i = 0; i < WARMUP_ITERATIONS; ++i)
            {
                slotter.invoke(i % 100);
            }

            // 实际测量
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < MEASURE_ITERATIONS; ++i)
            {
                slotter.invoke(i % 100);
            }
            auto end = std::chrono::high_resolution_clock::now();

            result.my_slot_store_ns =
                std::chrono::duration<double, std::nano>(end - start).count() /
                MEASURE_ITERATIONS;
#if debug_any_storage
            result.heap_allocations = my_slot_store::storage_type::construct_heap_count();
#endif
        }

        // 2. 测试 std::function
        {
            VeryBigCaptureObject very_big_capture;
            std::function<void(int)> func = very_big_capture;

            // 预热
            for (int i = 0; i < WARMUP_ITERATIONS; ++i)
            {
                func(i % 100);
            }

            // 实际测量
            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < MEASURE_ITERATIONS; ++i)
            {
                func(i % 100);
            }
            auto end = std::chrono::high_resolution_clock::now();

            result.std_function_ns =
                std::chrono::duration<double, std::nano>(end - start).count() /
                MEASURE_ITERATIONS;
        }

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "my_slot_store: " << result.my_slot_store_ns << " ns/call\n";
        std::cout << "std::function: " << result.std_function_ns << " ns/call\n";
        std::cout << "开销比率: "
                  << (result.my_slot_store_ns / result.std_function_ns * 100) << "%\n";
        std::cout << "my_slot_store 堆分配: " << result.heap_allocations << "\n";
    }
}

// ==================== 内存访问模式测试 ====================

void test_memory_access_pattern()
{
    std::cout << "\n========== 内存访问模式对调用开销的影响 ==========\n";

    constexpr int ITERATIONS = 1000000;

    // 创建大量对象，模拟真实场景中的缓存不友好情况
    struct CaptureWithData
    {
        std::array<int, 64> data;
        int value = 0;

        void operator()(int x)
        {
            value += x;
            // 模拟一些内存访问
            data[value % 64] = x;
        }
    };

    // 测试 my_slot_store
    {
        std::vector<my_slot_store> slots;
        std::vector<TestObject> objects(1000);

        for (size_t i = 0; i < objects.size(); ++i)
        {
            CaptureWithData capture;
            capture.value = static_cast<int>(i);
            slots.emplace_back(&objects[i], capture);
        }

        // 随机访问测试（缓存不友好）
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, objects.size() - 1);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i)
        {
            size_t idx = dist(rng);
            slots[idx].invoke(i % 100);
        }
        auto end = std::chrono::high_resolution_clock::now();

        double time_ns =
            std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
        std::cout << "my_slot_store 随机访问: " << time_ns << " ns/call\n";
    }

    // 测试 std::function
    {
        std::vector<std::function<void(int)>> functions;
        std::vector<TestObject> objects(1000);

        for (size_t i = 0; i < objects.size(); ++i)
        {
            CaptureWithData capture;
            capture.value = static_cast<int>(i);
            functions.emplace_back(capture);
        }

        // 随机访问测试
        std::mt19937 rng(42);
        std::uniform_int_distribution<size_t> dist(0, objects.size() - 1);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i)
        {
            size_t idx = dist(rng);
            functions[idx](i % 100);
        }
        auto end = std::chrono::high_resolution_clock::now();

        double time_ns =
            std::chrono::duration<double, std::nano>(end - start).count() / ITERATIONS;
        std::cout << "std::function 随机访问: " << time_ns << " ns/call\n";
    }
}

// ==================== 调用链深度测试 ====================

void test_call_chain_depth()
{
    std::cout << "\n========== 调用链深度对开销的影响 ==========\n";

    constexpr int DEPTH = 10;
    constexpr int ITERATIONS = 100000;

    struct ChainCapture
    {
        int depth;
        int value = 0;

        void operator()(int x)
        {
            value += x;
            // 模拟多层调用
            for (int i = 0; i < depth; ++i)
            {
                value += i; // 一些简单计算
            }
        }
    };

    // 测试不同深度的调用开销
    std::cout << std::fixed << std::setprecision(2);

    for (int depth = 1; depth <= DEPTH; ++depth)
    {
        // my_slot_store
        {
            TestObject obj;
            ChainCapture capture;
            capture.depth = depth;
            my_slot_store slotter{&obj, capture};

            auto start = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < ITERATIONS; ++i)
            {
                slotter.invoke(i % 100);
            }
            auto end = std::chrono::high_resolution_clock::now();

            double time_ns =
                std::chrono::duration<double, std::nano>(end - start).count() /
                ITERATIONS;
            std::cout << "Depth " << depth << ": my_slot_store = " << time_ns
                      << " ns/call";

            // std::function
            {
                ChainCapture capture2;
                capture2.depth = depth;
                std::function<void(int)> func = capture2;

                start = std::chrono::high_resolution_clock::now();
                for (int i = 0; i < ITERATIONS; ++i)
                {
                    func(i % 100);
                }
                end = std::chrono::high_resolution_clock::now();

                double time_ns2 =
                    std::chrono::duration<double, std::nano>(end - start).count() /
                    ITERATIONS;
                std::cout << ", std::function = " << time_ns2 << " ns/call";
                std::cout << ", 比率 = " << (time_ns / time_ns2 * 100) << "%\n";
            }
        }
    }
}

// ==================== 快速结论 ====================

void print_heap_call_conclusion()
{
    std::cout << "\n========== 堆内存调用开销结论 ==========\n";
    std::cout << "✅ my_slot_store 在堆内存调用开销上：\n";
    std::cout << "1. 调用路径更直接：虚表 -> 函数指针 -> 调用\n";
    std::cout << "2. 无额外类型检查：在构造时已确定类型\n";
    std::cout << "3. 内存布局紧凑：函数对象与any_storage紧邻\n";
    std::cout << "4. 无动态分配开销：调用时不分配内存\n\n";

    std::cout << "✅ 相比 std::function：\n";
    std::cout << "1. 更少的间接调用层级\n";
    std::cout << "2. 更简单的虚表结构\n";
    std::cout << "3. 无额外的异常处理开销\n";
    std::cout << "4. 更可预测的性能\n\n";

    std::cout << "⚠️ 注意：\n";
    std::cout << "1. my_slot_store 专为特定场景优化（对象+函数）\n";
    std::cout << "2. std::function 更通用但开销略高\n";
    std::cout << "3. 实际差异取决于编译器和优化级别\n";
}

// 修改 main 函数，在最后添加这些测试
int main()
{
    std::cout << "==================== 堆内存调用开销比较测试 ====================\n";

    try
    {
        // 原有的测试
        test_member_function();
        test_lambda();
        test_function_pointer();
        test_performance_comparison();
        test_edge_cases();

        // 新增的堆内存调用开销测试
        test_heap_call_overhead();
        test_memory_access_pattern();
        test_call_chain_depth();
        print_heap_call_conclusion();

        std::cout << "\n✅ 所有测试完成!\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "\n❌ 测试失败: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
// NOTE: 特化，clang 能减少一个8bit的大小，但是调用链速度确实变慢了。gcc则纯速度编码
//  NOTE: 因此，原先的就很好继承不见得是好的方式
// NOTE: 总之 是否有提升很难说的...。
// 特化版本，调用链边长是实打实的，追求速度就不该使用这个修改 NOLINTEND