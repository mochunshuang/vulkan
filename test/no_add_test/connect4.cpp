#include <bit>
#include <cassert>
#include <cstddef>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <cstring>
#include <type_traits>

// NOLINTBEGIN

template <typename T>
struct traits_lambda;

template <typename R, typename... Args>
struct traits_lambda<R (*)(Args...)>
{
    using return_type = R;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    using no_cvref_args_tuple = std::tuple<std::remove_cvref_t<Args>...>;
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
    using no_cvref_args_tuple = std::tuple<std::remove_cvref_t<Args>...>;
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
    using no_cvref_args_tuple = std::tuple<std::remove_cvref_t<Args>...>;
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
    using no_cvref_args_tuple = std::tuple<std::remove_cvref_t<Args>...>;
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
    using no_cvref_args_tuple = std::tuple<std::remove_cvref_t<Args>...>;
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
    using no_cvref_args_tuple = std::tuple<std::remove_cvref_t<Args>...>;
    static constexpr bool is_noexcept = true;
    static constexpr bool is_const = true;
    static constexpr bool is_static = false;
    static constexpr size_t args_size = sizeof...(Args);
};

template <typename no_cvref_args_tuple>
struct parma_0_args_tuple;

template <typename T0, typename... Remain>
struct parma_0_args_tuple<std::tuple<T0, Remain...>>
{
    using type = T0;
};

template <typename Func>
struct traits_slot_function;

template <typename T, typename... Args>
struct traits_slot_function<void (*)(T, Args...) noexcept>
{
    static_assert(std::is_object_v<std::remove_pointer_t<std::remove_cvref_t<T>>>,
                  "not member_function_pointer");
    static constexpr bool is_pointer = true;
    static constexpr size_t args_size = sizeof...(Args);
    static constexpr bool is_noexcept = true;
    using param_0 = T;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    using no_cvref_args_tuple = std::tuple<std::remove_cvref_t<Args>...>;
};

template <>
struct traits_slot_function<void (*)() noexcept>
{

    static constexpr bool is_pointer = true;
    static constexpr size_t args_size = 0;
    static constexpr bool is_noexcept = true;
    using param_0 = void;
    using args_tuple = std::tuple<>;
    using ref_args_tuple = std::tuple<>;
    using no_cvref_args_tuple = std::tuple<>;
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
    using no_cvref_args_tuple = typename traits::no_cvref_args_tuple;
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
                    using traits = traits_slot_function<Func_t>;
                    using tuple = traits::ref_args_tuple;
                    using no_cvref_args_tuple = traits::no_cvref_args_tuple;
                    tuple &args_tupe = *std::bit_cast<tuple *>(parms);

                    static_assert(traits::is_noexcept, "noexcept");

                    Sloter *obj = static_cast<Sloter *>(self->obj_);
                    Func_t &fun_ref = *(self->template get_pointer<Func_t>(self));
                    if constexpr (traits::is_pointer)
                    {
                        static_assert(std::is_pointer_v<Func_t>,
                                      "should ponter function.");
                        static_assert(sizeof(Func_t) == sizeof(void *),
                                      "make sure member_function with deducing this");

                        [&]<size_t... I>(std::index_sequence<I...>) constexpr noexcept {
                            if constexpr (std::is_invocable_v<
                                              Func_t &, std::tuple_element_t<
                                                            I, no_cvref_args_tuple>...>)
                                (fun_ref)(std::move(std::get<I>(args_tupe))...);
                            else if constexpr (std::is_invocable_v<
                                                   Func_t &, Sloter &,
                                                   std::tuple_element_t<
                                                       I, no_cvref_args_tuple>...>)
                                // c++23 member_function with deducing this
                                (fun_ref)(*obj, std::move(std::get<I>(args_tupe))...);

                            else
                                static_assert(
                                    false,
                                    "Cannot invoke function with provided arguments. "
                                    "Supported for (is_pointer==true):\n"
                                    "1. void(Args...) when match static function\n"
                                    "2. void(Obj&,Args...) when match member_function");
                        }(std::make_index_sequence<traits::args_size>{});
                    }
                    else
                    {
                        static_assert(std::is_object_v<Func_t>,
                                      "should lambda function.");
                        [&]<size_t... I>(std::index_sequence<I...>) constexpr noexcept {
                            if constexpr (std::is_invocable_v<
                                              Func_t &, std::tuple_element_t<
                                                            I, no_cvref_args_tuple>...>)
                                (fun_ref)(std::move(std::get<I>(args_tupe))...);
                            else
                                static_assert(
                                    false,
                                    "Cannot invoke function with provided arguments. "
                                    "Supported for (is_pointer==false):\n"
                                    "1. void(Args...) when match lambda function\n");
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
    constexpr any_storage(Sloter *sloter, Obj &&obj, Allocator alloc = {})
        : ops_{create_ops<std::decay_t<Sloter>, std::decay_t<Obj>,
                          std::decay_t<Allocator>>()},
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
};

//-------------------------------------------------------------

struct my_slot_store final : any_storage<2 * sizeof(void *), alignof(std::max_align_t)>
{
    using storage_type = any_storage<2 * sizeof(void *), alignof(std::max_align_t)>;
    using base_type = storage_type;
    using base_type::base_type;
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
    void onClick4(this slot_object &self) noexcept
    {
        std::cout << "click: " << 4 << '\n';
        self.value = 4;
    }
    int value;
};

static bool call_void_func = false;
void void_func() noexcept
{
    std::cout << "void_func...";
    call_void_func = true;
}

void void_func2(slot_object &obj) noexcept
{
    obj.value = 100;
}

void test()
{
    slot_object obj;

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
        my_slot_store sloter{&obj, &slot_object::onClick4};
        sloter.invoke();
        assert(obj.value == 4);
    }
    {
        assert(call_void_func == false);
        my_slot_store sloter{&obj, &void_func};
        sloter.invoke();
        assert(call_void_func == true);
    }
    {
        // c0:这可能是 BUG 的来源。 siganl 和 slot 完全匹配。才是合理的，不要自作聪明
        my_slot_store sloter{&obj, &void_func2};
        sloter.invoke(); // NOTE: 变体。 signal 可以变体
        assert(obj.value == 100);
    }

    auto lambda4 = [](int) {
    };

    // // NOTE: 2. 测试lambda
    static_assert(my_slot_store::storage_type::is_small<
                  decltype([&obj, &lambda4](int i) constexpr noexcept {
                      obj.onClick2(i);
                      lambda4(i);
                  })>());
    my_slot_store sloter2{
        &obj, [&obj, &lambda4](int i) constexpr noexcept { obj.onClick2(i); }};
    sloter2.invoke(20);
    assert(obj.value == 20);

    // // NOTE: 3. 测试纯指针
    my_slot_store sloter3{&obj, [ptr = &obj](slot_object *obj, int i) constexpr noexcept {
                              assert(ptr == obj && "check fail ptr == obj");
                              obj->value = i;
                          }};

    sloter3.invoke(&obj, 30);
    assert(obj.value == 30);

    // // NOTE: 构造的时候就 强转 必须按签名传递。 隐式只会害了你
    // //  NOTE: 4. 允许获得 obj 指针，不需要显示 invoke的时候，添加 &obj

    // lambda pointer
    my_slot_store sloter4{
        &obj, [](slot_object *obj, int i) static constexpr noexcept { obj->value = i; }};
    sloter4.invoke(&obj, 50);
    assert(obj.value == 50);

    // lambda with  capture
    my_slot_store sloter6{&obj, [&](int i) constexpr noexcept { obj.value = i; }};
    sloter6.invoke(60);
    assert(obj.value == 60);

    std::cout << "✅ test\n";
}

int main()
{
    test();
    std::cout << "main done\n";
    return 0;
}
// NOLINTEND