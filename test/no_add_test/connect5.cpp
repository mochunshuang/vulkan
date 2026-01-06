#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <iostream>
#include <set>
#include <shared_mutex>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <cstring>
#include <type_traits>
#include <variant>
#include <vector>

// NOLINTBEGIN

template <typename T>
struct slot_function;

//---------------- pointer type -----------------------

template <typename T>
concept valid_slot = requires { typename T::sloter_type; };

// 计算指针深度
template <typename T>
struct pointer_depth : std::integral_constant<size_t, 0>
{
};
template <typename T>
struct pointer_depth<T *> : std::integral_constant<size_t, pointer_depth<T>::value + 1>
{
};
template <typename T>
struct pointer_depth<const T> : pointer_depth<T>
{
};
template <typename T>
struct pointer_depth<volatile T> : pointer_depth<T>
{
};
template <typename T>
struct pointer_depth<const volatile T> : pointer_depth<T>
{
};
template <typename T>
inline constexpr size_t pointer_depth_v = pointer_depth<T>::value;

template <typename T>
struct ref_depth : std::integral_constant<size_t, 0>
{
};
template <typename T>
struct ref_depth<T &> : std::integral_constant<size_t, ref_depth<T>::value + 1>
{
};
template <typename T>
struct ref_depth<const T> : ref_depth<T>
{
};
template <typename T>
struct ref_depth<volatile T> : ref_depth<T>
{
};
template <typename T>
struct ref_depth<const volatile T> : ref_depth<T>
{
};
template <typename T>
inline constexpr size_t ref_depth_depth_v = ref_depth<T>::value;

template <typename T>
concept row_pointer = pointer_depth_v<T> == 1;

template <typename T>
concept row_lref = ref_depth_depth_v<T> == 1;

static_assert(row_lref<int &>);
static_assert(row_pointer<int *>);
static_assert(!row_pointer<int **>);
static_assert(!row_lref<int &&>);

template <typename T>
concept undefined_function = !requires(T t) { auto(&t); };

template <typename... T>
concept no_cvref = (std::is_same_v<T, std::remove_cvref_t<T>> && ...);
static_assert(no_cvref<>);

template <typename T>
struct valid_signal_impl
{
    static constexpr bool value = false;
};

// NOTE: 不需要移动赋值安全，因为不会操作中间值
template <no_cvref... Args>
    requires((std::is_nothrow_move_constructible_v<Args>) && ...)
struct valid_signal_impl<void(Args...)>
{
    static_assert(no_cvref<Args...>);
    static constexpr bool value = true;
};
template <no_cvref... Args>
    requires((std::is_nothrow_move_constructible_v<Args>) && ...)
struct valid_signal_impl<void(Args...) noexcept>
{
    static_assert(no_cvref<Args...>);
    static constexpr bool value = true;
};
template <typename T, typename... Args>
struct valid_signal_args_impl
{
    static constexpr bool value = false;
};
template <no_cvref... A, no_cvref... B>
struct valid_signal_args_impl<void(A...), B...>
{
    static constexpr bool value = (std::is_same_v<A, B> && ...);
};
template <no_cvref... A, no_cvref... B>
struct valid_signal_args_impl<void(A...) noexcept, B...>
{
    static constexpr bool value = (std::is_same_v<A, B> && ...);
};

template <typename T>
concept valid_signal = std::is_function_v<T> && valid_signal_impl<T>::value;

template <typename T, typename... Args>
concept valid_signal_args = valid_signal<T> && valid_signal_args_impl<T, Args...>::value;

template <typename Sloter, no_cvref... Args>
    requires(row_pointer<Sloter> || row_lref<Sloter>)
struct slot_function<void (*)(Sloter, Args...) noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr bool is_const = false;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename Sloter>
    requires(row_pointer<Sloter> || row_lref<Sloter>)
struct slot_function<void (*)(Sloter) noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<>;
    using ref_args_tuple = std::tuple<>;
    static constexpr bool is_const = false;
    static constexpr size_t args_size = 0;
};
static_assert(valid_slot<slot_function<void (*)(int *, int) noexcept>>);    // ✅ true
static_assert(!valid_slot<slot_function<void (*)(int *, int &) noexcept>>); // ✅ false

//---------------- lambda type -----------------------
template <typename lambda, typename Sloter, no_cvref... Args>
    requires(row_pointer<Sloter>)
struct slot_function<void (lambda ::*)(Sloter, Args...) noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr bool is_const = false;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename lambda, typename Sloter, no_cvref... Args>
    requires(row_pointer<Sloter>)
struct slot_function<void (lambda ::*)(Sloter, Args...) const noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr bool is_const = false;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename lambda, typename Sloter>
    requires(row_pointer<Sloter>)
struct slot_function<void (lambda ::*)(Sloter) noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<>;
    using ref_args_tuple = std::tuple<>;
    static constexpr bool is_const = false;
    static constexpr size_t args_size = 0;
};

template <typename lambda, typename Sloter>
    requires(row_pointer<Sloter>)
struct slot_function<void (lambda ::*)(Sloter) const noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<>;
    using ref_args_tuple = std::tuple<>;
    static constexpr bool is_const = false;
    static constexpr bool is_static = true;
    static constexpr size_t args_size = 0;
};

template <typename T>
    requires(
        std::is_object_v<T> &&
            requires() {
                typename slot_function<decltype(&T::operator())>::sloter_type;
            } ||
        std::is_pointer_v<T> && !std::is_member_function_pointer_v<T> &&
            requires() { typename slot_function<T>::sloter_type; })
struct traits_slot
{
    using function_type = decltype([]() consteval {
        if constexpr (requires() { &T::operator(); })
            return static_cast<decltype(&T::operator())>(nullptr);
        else
            return static_cast<T>(nullptr);
    }());
    static constexpr bool is_lambda = std::is_object_v<T>;
    using traits = slot_function<function_type>;
    using sloter_type = traits::sloter_type;
    using args_tuple = typename traits::args_tuple;
    using ref_args_tuple = typename traits::ref_args_tuple;
    static constexpr bool is_noexcept = traits::is_noexcept;
    static constexpr bool is_const = traits::is_const;
    static constexpr size_t args_size = traits::args_size;
};

template <std::size_t buffer_size, size_t align_size = alignof(std::max_align_t),
          typename allocator_storage_type = std::allocator<void>>
struct any_storage
{

    struct storage_ops
    {
        void (*destroy)(any_storage &self) noexcept;
        void (*copy_construct)(any_storage &dest, const any_storage &src);
        void (*move_construct)(any_storage &dest, any_storage &src) noexcept;
        bool (*equals)(const any_storage &a, const any_storage &b) noexcept;
        const std::type_info &(*type_info_T)() noexcept;
        const std::type_info &(*type_info_Allocator)() noexcept;
        // slot func
        void (*invoke)(any_storage *self, void *parms) noexcept;
        const std::type_info &(*type_info_O)() noexcept;
        bool (*stack_store)() noexcept;
    };

    const storage_ops *ops_ = nullptr;
    void *obj_;

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
    static constexpr const std::type_info &type_info_T_impl() noexcept
    {
        return typeid(T);
    }

    //------------------------vtable end----------------------------

    template <typename Sloter, typename T, typename Allocator>
        requires valid_slot<traits_slot<T>>
    static constexpr const storage_ops *create_ops() noexcept
    {
        static const auto vt = storage_ops{
            .destroy = &destroy_impl<T, Allocator>,
            .copy_construct = &copy_construct_impl<T, Allocator>,
            .move_construct = &move_construct_impl<T, Allocator>,
            .equals = &equals_impl<T>,
            .type_info_T = &type_info_T_impl<T>,
            .type_info_Allocator = &type_info_T_impl<Allocator>,
            .invoke =
                [](any_storage *self, void *parms) static constexpr noexcept {
                    using Func_t = std::remove_cvref_t<T>;
                    static_assert(not std::is_pointer_v<Sloter>);

                    using traits = traits_slot<Func_t>;
                    using sloter_type = traits::sloter_type;
                    using tuple_ref = traits::ref_args_tuple;
                    using args_tuple = traits::args_tuple;
                    tuple_ref &args_tupe = *std::bit_cast<tuple_ref *>(parms);

                    static_assert(row_pointer<Sloter *>);

                    Sloter *obj = static_cast<Sloter *>(self->obj_);
                    Func_t &fun_ref = *(self->template get_pointer<Func_t>(self));

                    if constexpr (std::is_pointer_v<sloter_type>)
                    {
                        static_assert(std::is_same_v<std::remove_cvref_t<sloter_type>,
                                                     std::remove_cvref_t<Sloter *>>);
                        [&]<size_t... I>(std::index_sequence<I...>) constexpr noexcept {
                            if constexpr (std::is_invocable_v<
                                              Func_t &, sloter_type,
                                              std::tuple_element_t<I, args_tuple>...>)
                                (fun_ref)(obj, std::move(std::get<I>(args_tupe))...);
                            else
                                static_assert(
                                    false,
                                    "Cannot invoke function with provided arguments. "
                                    "[void (Obj*,Args...) noexcept] is expected\n");
                        }(std::make_index_sequence<traits::args_size>{});
                    }
                    else
                    {
                        static_assert(row_lref<sloter_type>);
                        static_assert(std::is_same_v<std::remove_cv_t<sloter_type>,
                                                     std::remove_cv_t<Sloter &>>);
                        [&]<size_t... I>(std::index_sequence<I...>) constexpr noexcept {
                            if constexpr (std::is_invocable_v<
                                              Func_t &, sloter_type,
                                              std::tuple_element_t<I, args_tuple>...>)
                                (fun_ref)(*obj, std::move(std::get<I>(args_tupe))...);
                            else
                                static_assert(
                                    false,
                                    "Cannot invoke function with provided arguments. "
                                    "[void (Obj&,Args...) noexcept] is expected\n");
                        }(std::make_index_sequence<traits::args_size>{});
                    }
                },
            .type_info_O = &type_info_T_impl<Sloter>,
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

  public:
    constexpr any_storage() noexcept = delete;

    template <typename Sloter, typename Obj, typename Allocator = std::allocator<void>>
        requires requires {
            create_ops<std::decay_t<Sloter>, std::decay_t<Obj>,
                       std::decay_t<Allocator>>();
        }
    constexpr any_storage(Sloter *sloter, Obj &&obj, Allocator alloc = {})
        : ops_{create_ops<std::remove_cvref_t<Sloter>, std::remove_cvref_t<Obj>,
                          std::remove_cvref_t<Allocator>>()},
          obj_{sloter}, allocator_{alloc}
    {
        construct(std::forward<Obj>(obj), std::forward<Allocator>(alloc));
    }
    constexpr any_storage(const any_storage &other)
        : ops_(other.ops_), obj_{other.obj_}, allocator_{other.allocator_}
    {
        if (ops_)
            ops_->copy_construct(*this, other);
    }
    constexpr any_storage(any_storage &&other) noexcept
        : ops_(std::exchange(other.ops_, nullptr)), obj_{other.obj_},
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

            ops_ = other.ops_;
            obj_ = other.obj_;
            allocator_ = other.allocator_;
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

            ops_ = std::exchange(other.ops_, nullptr);
            obj_ = std::exchange(other.obj_, nullptr);
            allocator_ = std::move(other.allocator_);

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
    template <typename... Args>
    constexpr void invoke(Args... args) & noexcept
    {
        using args_tuple = std::tuple<Args &...>;
        args_tuple args_ = {args...};
        ops_->invoke(this, &args_);
    }
    [[nodiscard]] constexpr const std::type_info &object_type() const noexcept
    {
        return ops_->type_info_O();
    }
    [[nodiscard]] constexpr const std::type_info &function_type() const noexcept
    {
        return ops_->type_info_T();
    }
    [[nodiscard]] constexpr bool stack_store() const noexcept
    {
        return ops_->stack_store();
    }
    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return ops_ != nullptr;
    }
};

//-------------------------------------------------------------
struct ThrowOnMove
{
    ThrowOnMove() = default;
    ThrowOnMove(ThrowOnMove &&)
    { /* 可能抛异常 */
    }
};

struct NoThrowOnMove
{
    NoThrowOnMove() = default;
    NoThrowOnMove(NoThrowOnMove &&) noexcept = default;
};

struct ImplicitNoThrow
{
    ImplicitNoThrow() = default;
    // 隐式声明为 noexcept（如果所有成员都 noexcept）
    int x;
    double y;
};

struct OnlyDeleteMoveConstructible
{
    OnlyDeleteMoveConstructible() = default;
    OnlyDeleteMoveConstructible(OnlyDeleteMoveConstructible &&) noexcept = delete;
};
struct OnlyDeleteMoveAssignable
{
    OnlyDeleteMoveAssignable() = default;
    OnlyDeleteMoveAssignable(OnlyDeleteMoveAssignable &&) noexcept = default;
    OnlyDeleteMoveAssignable &operator=(OnlyDeleteMoveAssignable &&) noexcept = delete;
};

struct sloter_store final : any_storage<2 * sizeof(void *), alignof(std::max_align_t)>
{
    using storage_type = any_storage<2 * sizeof(void *), alignof(std::max_align_t)>;
    using base_type = storage_type;
    using base_type::base_type;
};

struct signal_object
{
    void click(int i); // NOTE: 这样不好

    using signal_click = void(int i); // NOTE: 这样保证这个是类型不是指针就OK了
    using signal_click2 = void(int i) noexcept;
    using signal_ref = void(int &i) noexcept;
    using signal_const = void(int const i) noexcept;
    using signal_const2 = void(const ImplicitNoThrow i) noexcept;
    using signal_const3 = void(const ImplicitNoThrow *i) noexcept;
    using signal_pointer = void(int *i) noexcept;
    using signal_obj = void(int *i, signal_object) noexcept;

    using signal_ThrowOnMove = void(ThrowOnMove) noexcept;
    using signal_NoThrowOnMove = void(NoThrowOnMove) noexcept;
    using signal_ImplicitNoThrow = void(ImplicitNoThrow) noexcept;

    using signal_OnlyDeleteMoveConstructible = void(OnlyDeleteMoveConstructible) noexcept;
    using signal_OnlyDeleteMoveAssignable = void(OnlyDeleteMoveAssignable) noexcept;

    static_assert(not std::is_pointer_v<signal_click>);
    static_assert(not std::is_pointer_v<decltype(&signal_object::click)>);
    static_assert(not std::is_member_object_pointer_v<decltype(&signal_object::click)>);

    // 检查是否是纯函数类型（不是指针、引用等）
    static_assert(std::is_function_v<signal_click>); // ✅ true
    static_assert(!std::is_function_v<
                  decltype(&signal_object::click)>);   // ❌ 错误：click是成员函数声明
    static_assert(!std::is_function_v<void (*)(int)>); // ✅ false：函数指针
    static_assert(!std::is_function_v<void (&)(int)>); // ✅ false：函数引用

    // 检查成员函数指针
    static_assert(
        std::is_member_function_pointer_v<decltype(&signal_object::click)>); // ✅
                                                                             // true
};
struct slot_object
{
    void onClick(this slot_object &self, int i) noexcept
    {
        std::cout << "click: " << i << '\n';
        self.value = i;
    }
    void onClick5(this const slot_object &self, int i) noexcept
    {
        std::cout << "click: " << i << '\n';
    }

    //----------Ordinary member function-----------
    void onClick2(slot_object *self, int i) noexcept
    {
        std::cout << "click: " << i << '\n';
        self->value = i;
    }
    void onClick6(const slot_object *self, int i) noexcept
    {
        std::cout << "click: " << i << '\n';
    }

    // ------------------ miss noexcept ------------------
    void onClick3(slot_object *self, int i)
    {
        std::cout << "click: " << i << '\n';
        self->value = i;
    }
    void onClick4(this slot_object &self, int i)
    {
        std::cout << "click: " << i << '\n';
        self.value = i;
    }

    // ------------------static function ------------------
    static void onClick7(slot_object *self, int i) noexcept
    {
        std::cout << "click: " << i << '\n';
        self->value = i;
    }
    static void onClick8(const slot_object *self, int i) noexcept
    {
        std::cout << "click: " << i << '\n';
    }

    static void onClick9(const slot_object *self, int i) // noexcept
    {
        std::cout << "click: " << i << '\n';
    }
    int value;
};
static void onClick10(const slot_object *self, int i) noexcept
{
    std::cout << "click: " << i << '\n';
}
static void parm0(const slot_object *self) noexcept {}
static void no_parm() noexcept {}

void test_slot_function()
{
    // c0: -------------pointer----------------
    static_assert(valid_slot<traits_slot<decltype(&slot_object::onClick)>>);
    // static_assert(valid_slot<traits_slot<decltype(&slot_object::onClick2)>>);
    // //fail

    // static_assert(not
    // valid_slot<traits_slot<decltype(&slot_object::onClick3)>>);//fail
    // static_assert(not
    // valid_slot<traits_slot<decltype(&slot_object::onClick4)>>);//fail

    static_assert(valid_slot<traits_slot<decltype(&slot_object::onClick5)>>);
    // static_assert(valid_slot<traits_slot<decltype(&slot_object::onClick6)>>);//fail

    static_assert(valid_slot<traits_slot<decltype(&slot_object::onClick7)>>);
    static_assert(valid_slot<traits_slot<decltype(&slot_object::onClick8)>>);
    // static_assert(valid_slot<traits_slot<decltype(&slot_object::onClick9)>>);//fail

    // glob noexcept
    static_assert(valid_slot<traits_slot<decltype(&::onClick10)>>); // ok

    static_assert(valid_slot<traits_slot<decltype(&::parm0)>>); // ok

    // C0: first param is sloter always. so case of parm'count ==0 must fail
    //  static_assert(valid_slot<traits_slot<decltype(&::no_parm)>>); // fail

    // c1: -------------lambda----------------
    // no capterue
    static_assert(valid_slot<traits_slot<decltype([](slot_object *) noexcept {})>>);
    static_assert(
        valid_slot<traits_slot<decltype([](slot_object *) static noexcept {})>>);
    static_assert(valid_slot<
                  traits_slot<decltype([](slot_object *) constexpr static noexcept {})>>);

    // miss noexcept must fail
    // static_assert(valid_slot<traits_slot<decltype([](slot_object *) {})>>);

    // NOTE: case of parm'count ==0 must fail
    // static_assert(valid_slot<traits_slot<decltype([]() noexcept {})>>); // fail
    int a;
    static_assert(
        valid_slot<traits_slot<decltype([&](slot_object *self) noexcept { a = 10; })>>);

    static_assert(
        valid_slot<
            traits_slot<decltype([&](slot_object *self, int i) noexcept { a = i; })>>);

    // mutable is ok
    static_assert(
        valid_slot<traits_slot<decltype([=](slot_object *self, int i) mutable noexcept {
            a = i;
        })>>);

    //-----------------------------------------------------
    slot_object obj;
    using my_slot_store = sloter_store;

    // NOTE: 1. 测试成员函数
    my_slot_store sloter{&obj, &slot_object::onClick};

    static_assert(
        my_slot_store::storage_type::is_small<decltype(&slot_object::onClick)>());

    sloter.invoke(1);
    assert(obj.value == 1);
    sloter.invoke(10);
    assert(obj.value == 10);
    {
        // NOTE: 保证至少有一个参数？
        // my_slot_store sloter{&obj, &slot_object::onClick4}; // NOTE: 编译出错
        // sloter.invoke();
        // assert(obj.value == 4);
    }

    auto lambda4 = [](int) {
    };

    // NOTE: 2. 测试lambda
    my_slot_store sloter2{&obj, [&lambda4](slot_object *slef, int i) constexpr noexcept {
                              slef->onClick2(slef, i);
                          }};
    sloter2.invoke(20);
    assert(obj.value == 20);

    // NOTE: 3. 测试纯指针
    my_slot_store sloter3{
        &obj, [](slot_object *obj, int i) static constexpr noexcept { obj->value = i; }};
    sloter3.invoke(30);
    assert(obj.value == 30);
    {
        my_slot_store sloter3{&obj,
                              [ptr = &obj](slot_object *obj, int i) constexpr noexcept {
                                  assert(ptr == obj && "check fail ptr == obj");
                                  obj->value = i;
                              }};

        sloter3.invoke(40);
        assert(obj.value == 40);
    }

    // // // NOTE: 构造的时候就 强转 必须按签名传递。 隐式只会害了你
    // // //  NOTE: 4. 允许获得 obj 指针，不需要显示 invoke的时候，添加 &obj

    // c0: 上面已经使用这个 static的 lambda了。 clang 没有这个问题
    //  // lambda pointer //NOTE: gcc 有 abi 的问题： static 的lambda
    //  my_slot_store sloter4{
    //      &obj, [](slot_object *obj, int i) static constexpr noexcept { obj->value = i;
    //      }};
    //  sloter4.invoke(50); // NOTE: static 指针
    //  assert(obj.value == 50);

    // // // lambda with  capture
    my_slot_store sloter6{
        &obj, [](slot_object *obj, int i) constexpr noexcept { obj->value = i; }};
    sloter6.invoke(60);
    assert(obj.value == 60);

    // c0: 没有 static 则没有这个问题。不会重定义了
    auto fun = [](slot_object *obj, int i) constexpr noexcept {
        obj->value = i;
    };
    my_slot_store sloter7{&obj, std::move(fun)};
    sloter7.invoke(70);
    assert(obj.value == 70);

    // NOTE:统一显式 sloter 指针传递

    // NOTE: 信息：
    assert(sloter7.object_type() == typeid(slot_object));
    assert(sloter7.function_type() == typeid(decltype(fun)));

    struct slot_key
    {
        const std::type_info *obj;
        const std::type_info *slot;

        slot_key(const std::type_info &obj_type, const std::type_info &slot_type) noexcept
            : obj(&obj_type), slot(&slot_type)
        {
        }

        bool operator==(const slot_key &other) const noexcept
        {
            return *obj == *other.obj && *slot == *other.slot;
        }
    };
    struct slot_key_hash
    {
        size_t operator()(const slot_key &key) const noexcept
        {
            return key.obj->hash_code() ^ (key.slot->hash_code() << 1);
        }
    };
    // NOTE: slot 中心。封装成一个类？ 好像不错哦。
    std::unordered_map<slot_key, sloter_store, slot_key_hash> my_map;
    {
        int a = 0;
        int b = 1;
        assert(typeid(a) == typeid(b));   // NOTE: 这是类型层面的 还得需要指针
        assert((void *)&a != (void *)&b); // NOTE: 对象层
    }

    // NOTE: 如何存储： connect 对象 ？ signal ，sloter_store？确实是这样
    // NOTE: std::unordered_map<(void*)signal*,{(type_id)signal_id,sloter_store}>;
    // 这样岂不更好？ 那么connect是怎么回事？就是堆内存开销的过程？
    //
};
void test_signal()
{
    using T = decltype(&signal_object::click); // NOTE: 可以取得类型
    static_assert(
        std::is_same_v<void (signal_object::*)(int), T>); // NOTE: 成员函数的类型

    // C2: 只能是函数类型
    static_assert(valid_signal<signal_object::signal_click>);
    static_assert(valid_signal<signal_object::signal_click2>);

    static_assert(valid_signal<signal_object::signal_pointer>);
    static_assert(valid_signal<signal_object::signal_obj>);

    static_assert(!valid_signal<signal_object::signal_ref>); // NOTE: 必须是值语义

    // NOTE: 就离谱
    static_assert(valid_signal<signal_object::signal_const>); // const int 无效
    static_assert(std::is_same_v<void(const int) noexcept, signal_object::signal_const>);
    // NOTE: 这就是原因吗？？？？是的
    static_assert(std::is_same_v<void(int) noexcept, signal_object::signal_const>);

    static_assert(std::is_same_v<void(int) noexcept, void(const int) noexcept>);
    static_assert(
        std::is_same_v<void(NoThrowOnMove) noexcept, void(const NoThrowOnMove) noexcept>);
    static_assert(std::is_same_v<void(ImplicitNoThrow) noexcept,
                                 void(const ImplicitNoThrow) noexcept>);
    static_assert(!std::is_same_v<void(ImplicitNoThrow *) noexcept,
                                  void(const ImplicitNoThrow *) noexcept>);

    // NOTE: 变成函数也是相同的
    static_assert(std::is_same_v<void (*)(int) noexcept, void (*)(const int) noexcept>);

    static_assert(!std::is_same_v<int, const int>);
    static_assert(!std::same_as<int, const int>);
    static_assert(!std::is_same_v<int, int const>);
    static_assert(!std::same_as<int, int const>);

    // 这些断言都会通过
    static_assert(std::is_same_v<void(int), void(const int)>);      // true!
    static_assert(std::is_same_v<void(int *), void(int *const)>);   // true!
    static_assert(!std::is_same_v<void(int &), void(const int &)>); // false! 引用不同

    static_assert(valid_signal<signal_object::signal_const2>);
    static_assert(valid_signal<signal_object::signal_const3>);

    // NOTE: 值类型 const 参数是无效的

    static_assert(!valid_signal<signal_object::signal_ThrowOnMove>);
    static_assert(valid_signal<signal_object::signal_NoThrowOnMove>);
    static_assert(valid_signal<signal_object::signal_ImplicitNoThrow>);

    static_assert(!valid_signal<signal_object::signal_OnlyDeleteMoveConstructible>);
    static_assert(valid_signal<signal_object::signal_OnlyDeleteMoveAssignable>);

    std::cout << "✅ test\n";
}
void test_undefined_function()
{
    // NOTE: 是无法判断，生命的函数是否是未定义的。 因为声明和定义可以分开
    static_assert(!undefined_function<decltype(&signal_object::click)>);
}

void test_throw()
{
    static_assert(not std::is_nothrow_move_constructible_v<ThrowOnMove>);

    static_assert(std::is_nothrow_move_constructible_v<NoThrowOnMove>);

    static_assert(std::is_nothrow_move_constructible_v<ImplicitNoThrow>);

    static_assert(std::is_nothrow_move_constructible_v<int>);
    static_assert(std::is_nothrow_move_constructible_v<std::string>);
}

// NOTE: 0. 存放对象id
struct object_id final
{
    constexpr object_id(const void *ptr) noexcept : value{uintptr_t(ptr)} {}
    constexpr object_id(size_t v) noexcept : value{v} {}
    size_t value;
    bool operator==(const object_id &other) const noexcept = default;
    auto operator<=>(const object_id &b) const = default;

    template <typename signal_key>
    static constexpr object_id make_signal_id()
    {
        return object_id{typeid(signal_key).hash_code()};
    }
};
namespace std
{
    template <>
    struct hash<object_id>
    {
        size_t operator()(const object_id &id) const noexcept
        {
            // 使用指针地址作为哈希值
            return reinterpret_cast<size_t>(id.value);
        }
    };
}; // namespace std

struct connect_object;

struct connection_networks
{
  private:
    static auto &networks() noexcept
    {
        static std::unordered_set<connect_object *> active_object;
        return active_object;
    }

    static auto &mutex() noexcept
    {
        static std::shared_mutex mtx;
        return mtx;
    }

  public:
    static void online(connect_object *obj)
    {
        std::unique_lock lock(mutex());
        networks().insert(obj);
    }

    static void offline(connect_object *obj)
    {
        std::unique_lock lock(mutex());
        networks().erase(obj);
    }

    static bool is_online(connect_object *obj)
    {
        std::shared_lock lock(mutex());
        return networks().find(obj) != networks().end();
    }
};

// NOTE: 1 . sloterl_obj 或 signal_obj.都必须继承这个接口。 disonnect 时用到

// c0: 1. 永远是 signal -> slot。slot调用者永远是 signaler
// c0: 2. sloter 先析构，则需要告知 signaler
// c0: 3. 只有 sloter 才能主动建立 signla-slot 和 断开 siganl-slot
// c0: 4. signaler 先析构，什么都不用做。
// c0: 5. emit 的时候，才真正的 断开 siganl-slot,有延迟
// c0: 6. 建立连接就应该保持到析构。否则没必要说实话
struct connect_object
{
    using sloter_store_type = any_storage<2 * sizeof(void *), alignof(std::max_align_t)>;

    //--------------------------------recever_type------------------------------------------------
    struct rcvr_status
    {
        constexpr rcvr_status() = default;
        constexpr explicit rcvr_status(bool enable) noexcept : connect_ability_(enable) {}
        rcvr_status(const rcvr_status &) = default;
        rcvr_status &operator=(const rcvr_status &) = default;
        constexpr rcvr_status(rcvr_status &&o) noexcept
            : connect_ability_{std::exchange(o.connect_ability_, {})}
        {
        }
        constexpr rcvr_status &operator=(rcvr_status &&o) noexcept
        {
            if (&o != this)
                connect_ability_ = std::exchange(o.connect_ability_, {});
            return *this;
        };
        ~rcvr_status() = default;

        friend void swap(rcvr_status &a, rcvr_status &b) noexcept
        {
            std::swap(a.connect_ability_, b.connect_ability_);
        }

        [[nodiscard]] constexpr bool connectable() const noexcept
        {
            return connect_ability_;
        }
        constexpr void enable_connect() noexcept
        {
            connect_ability_ = true;
        }
        constexpr void disable_connect() noexcept
        {
            connect_ability_ = false;
        }
        bool operator==(const rcvr_status &o) const noexcept = default;
        auto operator<=>(const rcvr_status &b) const = default;

      private:
        bool connect_ability_{};
    };
    [[nodiscard]] constexpr bool connectable() const noexcept
    {
        return status.connectable();
    }
    constexpr void enable_connect() noexcept
    {
        status.enable_connect();
    }
    constexpr void disable_connect() noexcept
    {
        status.disable_connect();
    }
    [[nodiscard]] constexpr sloter_store_type *connect_sndr(connect_object *sndr,
                                                            sloter_store_type slot)
    {
        if (not connectable())
            return nullptr;
        sndr_connected.insert(sndr);
        slots.emplace_back(std::move(slot));
        return &slots.back(); // NOTE: BUG 的来源
    }
    constexpr sloter_store_type *last_connection() noexcept
    {
        return &slots.back();
    }
    constexpr void disconnect_sndr(connect_object *sndr)
    {
        sndr_connected.erase(sndr);
    }

    rcvr_status status{};
    std::set<connect_object *> sndr_connected; // NOTE: 如果signal 先死这就有问题了
    std::vector<sloter_store_type> slots;
    //-----------------------------------------------------------------------------------------

    //-----------------------------------sender_type--------------------------------------------------
    struct slot_type
    {
        connect_object *recr;
        sloter_store_type *slot;
        bool operator==(const slot_type &o) const noexcept = default;
        auto operator<=>(const slot_type &b) const = default;
    };
    struct delete_signal_key
    {
        object_id signal_id;
        connect_object *recr;
    };
    using delete_sloter_key =
        std::variant<slot_type, connect_object *, delete_signal_key>;

    constexpr void connect_rcvr(object_id signal_id, slot_type valid_slot)
    {
        signal_slot_map[signal_id].emplace_back(valid_slot);
        recr_signal_map[static_cast<connect_object *>(valid_slot.recr)].emplace_back(
            signal_id);
    }
    constexpr void connect_rcvr(object_id signal_id, connect_object *recr,
                                sloter_store_type *slot)
    {
        connect_rcvr(signal_id, {.recr = recr, .slot = slot});
    }

    void disconnect_slot(object_id signal_id, slot_type slot_ptr)
    {
        // delete one slot_ptr from signal_slot_map if match and then just recr_signal_map
        if (auto it = signal_slot_map.find(signal_id); it != signal_slot_map.end())
        {
            std::erase_if(it->second, [&](const slot_type &slot) noexcept {
                return slot_ptr == slot;
            });
            std::erase_if(recr_signal_map[slot_ptr.recr],
                          [&](const object_id &id) noexcept { return signal_id == id; });
        }
    }
    void disconnect_signal(delete_signal_key key)
    {
        auto [signal_id, recr] = key;
        // delete all slot that bind to recr and remove recr form recr_signal_map
        std::erase_if(signal_slot_map[signal_id],
                      [&](const slot_type &slot) noexcept { return slot.recr == recr; });
        std::erase_if(recr_signal_map[recr],
                      [&](const object_id &id) noexcept { return signal_id == id; });
    }
    void disconnect_rcvr(connect_object *recr)
    {
        recr_signal_map.erase(recr);
    }

    // NOTE: 单线程足够了
    //  c0: 一个删除 一个迭代。锁？
    void as_sndr_wait_delete_invalid_slot(object_id signal_id)
    {
        if (not disconnects_key.empty())
        {
            for (auto &key : disconnects_key)
                std::visit(
                    [&](auto &&arg) {
                        using T = std::decay_t<decltype(arg)>;
                        if constexpr (std::is_same_v<T, slot_type>)
                            disconnect_slot(signal_id, arg);
                        else if constexpr (std::is_same_v<T, connect_object *>)
                            disconnect_rcvr(arg);
                        else if constexpr (std::is_same_v<T, delete_signal_key>)
                            disconnect_signal(arg);
                        else
                            static_assert(false, "non-exhaustive visitor!");
                    },
                    key);
            disconnects_key.clear();
        }
    }

    template <typename signal_key, typename... Args>
        requires(valid_signal_args<signal_key, Args...>)
    void emit(Args... args)
    {
        object_id signal_id = object_id::make_signal_id<signal_key>();
        as_sndr_wait_delete_invalid_slot(signal_id);
        auto it = signal_slot_map.find(signal_id);
        if (it != signal_slot_map.end())
            for (slot_type &slot : it->second)
                slot.slot->invoke(std::move(args)...);
    }

    void disconnect_rcvr_lazy(connect_object *recvr)
    {
        disconnects_key.emplace_back(recvr);
    }
    void disconnect_slot_lazy(connect_object *recvr, sloter_store_type *slot)
    {
        disconnects_key.emplace_back(slot_type{.recr = recvr, .slot = slot});
    }

    std::vector<delete_sloter_key> disconnects_key;
    std::unordered_map<object_id, std::vector<slot_type>> signal_slot_map;
    std::unordered_map<connect_object *, std::vector<object_id>> recr_signal_map;
    //---------------------------------------------------------------------------------------

    // c0: emit是 sndr 线程 发起的。 recever 线程必须考虑多线程安全吗？
    // c0: emit 事件发起层 和 reveber层 不在一个线程？没关系吧。值语言呢
    // c0: signal slot 就是 signal 改变 slot 的状态啊。move 值语言不会有线程安全

    // ---------------------属性----------------------------------------
    constexpr auto &as_sndr() noexcept
    {
        return *this;
    }
    constexpr auto &as_rcvr() noexcept
    {
        return *this;
    }

    //-------------------------------sndr recr -------------------
    constexpr void rcvr_make_diconect()
    {
        as_rcvr().disable_connect();
        for (auto sndr : as_rcvr().sndr_connected)
            sndr->as_sndr().disconnect_rcvr_lazy(this);
        sndr_connected.clear();
    }
    constexpr void sndr_make_diconect()
    {
        for (auto key : as_sndr().disconnects_key)
            std::visit(
                [&](auto &&key) {
                    using T = std::decay_t<decltype(key)>;
                    if constexpr (std::is_same_v<T, slot_type>)
                        ;
                    else if constexpr (std::is_same_v<T, connect_object *>)
                        as_sndr().disconnect_rcvr(key);
                    else if constexpr (std::is_same_v<T, delete_signal_key>)
                        ;
                    else
                        static_assert(false, "non-exhaustive visitor!");
                },
                key);

        // make sure rest recr is valid
        for (auto &[recr, _] : as_sndr().recr_signal_map)
            if (recr->connectable())
                recr->as_rcvr().disconnect_sndr(this);
    };
    //-------------------------
    connect_object() = default;
    connect_object(const connect_object &) = default;
    connect_object(connect_object &&) = default;
    connect_object &operator=(const connect_object &) = default;
    connect_object &operator=(connect_object &&) = default;
    ~connect_object() noexcept
    {
        try
        {
            rcvr_make_diconect();
            sndr_make_diconect();
        }
        catch (...)
        {
        }
    }
    bool operator==(const connect_object &o) const noexcept = default;

    //-----------------------static function----------------------------
    static void disconnect(connect_object *signal_obj, connect_object *recvr)
    {
        signal_obj->as_sndr().disconnect_rcvr_lazy(recvr);
    }
    static void disconnect(connect_object *signal_obj, connect_object *recvr,
                           sloter_store_type *slot)
    {
        signal_obj->as_sndr().disconnect_slot_lazy(recvr, slot);
    }
    template <typename signal_key, typename Rcvr, typename Slot>
    [[nodiscard]] static auto connect(connect_object *sndr, Rcvr *revr, Slot slot)
        -> sloter_store_type *
    {
        if (sloter_store_type *slot_ptr = revr->as_rcvr().connect_sndr(
                sndr, sloter_store_type{revr, std::move(slot)});
            slot_ptr != nullptr)
        {
            sndr->as_sndr().connect_rcvr(object_id::make_signal_id<signal_key>(), revr,
                                         slot_ptr);
            return slot_ptr;
        }
        return nullptr;
    }
};

void test_connection()
{
    struct signal_enable
    {
        using signal_click = void(int i);
    };
    struct my_sloter
    {
        void onclick(my_sloter *self, int i) noexcept
        {
            self->value = i;
        }
        int value;
    };
}

void test_type_info()
{
    // sloter + slot_fu ？
    []() {
    };
    constexpr auto &t = typeid(decltype([]() {}));
}

void test_connect()
{
    //
    struct my_signal : connect_object
    {
        using signal_click = void(int key);
        void submit_key_event(int v = 0)
        {
            emit<signal_click>(v);
        }

        void onSuccessEmit(this my_signal &self) noexcept
        {
            std::cout << "successful emit\n";
            self.has_callback = true;
        }
        bool has_callback{};
    };
    struct my_slot : connect_object
    {
        using signal_click_callback = void();

        my_slot() noexcept
        {
            enable_connect(); // NOTE: 是否默认允许
        }
        void onClick(this my_slot &self, int key) noexcept
        {
            self.value = key;
            self.emit<signal_click_callback>();
        }
        int value{-1};
    };
    my_signal a;
    my_slot b;

    assert(not a.connectable());
    assert(b.connectable());

    auto *ptr =
        connect_object::connect<my_signal::signal_click>(&a, &b, &my_slot::onClick);
    assert(ptr != nullptr);

    a.submit_key_event();
    assert(b.value == 0);
    assert(a.has_callback == false);

    a.submit_key_event(1);
    assert(b.value == 1);
    assert(a.has_callback == false);

    // NOTE: make sure enable_connect before connect_object::connect
    a.enable_connect();
    ptr = connect_object::connect<my_slot::signal_click_callback>(
        &b, &a, &my_signal::onSuccessEmit);
    assert(ptr != nullptr);

    a.submit_key_event(2);
    assert(b.value == 2);
    assert(a.has_callback == true);

    // c0: lambda : 崩溃，因为索引在 vector 修正后失效。 值存储是错误的，因为扩容
    // a.has_callback = false;
    // ptr = connect_object::connect<my_signal::signal_click>(
    //     &a, &b, [](my_slot *b, int v) noexcept { b->value = v; });
    // assert(ptr != nullptr);
    // a.submit_key_event(3);
    // assert(b.value == 3);
    // assert(a.has_callback == false);
}

int main()
{
    test_signal();
    test_slot_function();
    test_connect();
    std::cout << "main done\n";
    return 0;
}
// NOLINTEND