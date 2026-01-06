#include <algorithm>
#include <atomic>
#include <bit>
#include <cassert>
#include <concepts>
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
    using args_tuple = std::tuple<Args...>;
};
template <no_cvref... Args>
    requires((std::is_nothrow_move_constructible_v<Args>) && ...)
struct valid_signal_impl<void(Args...) noexcept>
{
    static_assert(no_cvref<Args...>);
    static constexpr bool value = true;
    using args_tuple = std::tuple<Args...>;
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
    using full_ref_tuple = std::tuple<Sloter, Args &...>;
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
    using full_ref_tuple = std::tuple<Sloter>;
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
    using full_ref_tuple = std::tuple<Sloter, Args &...>;
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
    using full_ref_tuple = std::tuple<Sloter, Args &...>;
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
    using full_ref_tuple = std::tuple<Sloter>;
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
    using full_ref_tuple = std::tuple<Sloter>;
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
    using full_ref_tuple = typename traits::full_ref_tuple;
    static constexpr bool is_noexcept = traits::is_noexcept;
    static constexpr bool is_const = traits::is_const;
    static constexpr size_t args_size = traits::args_size;
    static constexpr bool sloter_type_is_pointer = std::is_pointer_v<sloter_type>;
};

// NOTE: 0. 存放对象id
struct object_id final
{
    constexpr object_id(const void *ptr) noexcept : value{uintptr_t(ptr)} {}
    constexpr object_id(size_t v) noexcept : value{v} {}
    size_t value;
    bool operator==(const object_id &other) const noexcept = default;
    auto operator<=>(const object_id &b) const = default;

    template <typename signal_key>
    static constexpr object_id make_signal_id() noexcept
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

/*
        using TS = valid_signal_impl<signal_click>::args_tuple;
        using TS1 = traits_slot<slot_type>::args_tuple;
*/
template <typename signal_click, typename slot_type>
concept signal_slot_match =
    std::same_as<typename valid_signal_impl<signal_click>::args_tuple,
                 typename traits_slot<slot_type>::args_tuple>;

struct slot_interface
{
    using invoke_slot_type = void(void *obj, void *args) noexcept;

    template <typename... Args>
    constexpr void invoke(Args &&...args) & noexcept
    {
        using args_tuple = std::tuple<Args &...>;
        args_tuple args_ = {args...};
        (slot)(obj, &args_);
    }

    slot_interface() = delete;
    template <typename T, typename Args>
    explicit slot_interface(T &o, Args *) noexcept
        : obj{&o}, slot{[](void *obj, void *args) constexpr noexcept {
              T *impl = static_cast<T *>(obj);
              Args &args_ = *static_cast<Args *>(args);
              std::apply(*impl, args_);
          }}
    {
    }

  private:
    void *obj;
    invoke_slot_type *slot;
};
template <typename Impl, typename ArgsTuple>
struct slot_impl;

template <typename Impl, typename... Args>
struct slot_impl<Impl, std::tuple<Args...>> : slot_interface
{
    slot_impl(Impl slot) noexcept
        : slot_interface{this->impl, static_cast<std::tuple<Args...> *>(nullptr)},
          impl{std::move(slot)} {};
    Impl impl;
};

struct my_signal
{
    using signal_click = void(int key);
    void submit_key_event(int v = 0)
    {
        // emit<signal_click>(v);
    }

    void onSuccessEmit(this my_signal &self) noexcept
    {
        std::cout << "successful emit\n";
        self.has_callback = true;
    }
    bool has_callback{};
};
struct my_slot
{

    using signal_click_callback = void();

    my_slot() noexcept {}
    void onClick(this my_slot &self, int key) noexcept
    {
        self.value = key;
        // self.emit<signal_click_callback>();
    }
    int value{-1};
};

void test_invoke()
{
    {
        my_slot s;
        my_slot &refs = s;
        my_slot &refref = refs;
    }
    using signal_click = void(int key);

    my_slot s;
    auto impl = std::make_shared<
        slot_impl<decltype(&my_slot::onClick), std::tuple<my_slot &, int &>>>(
        &my_slot::onClick);
    slot_interface *fun = impl.get();
    fun->invoke(s, 1);
    assert(s.value == 1);

    std::shared_ptr<slot_interface> base;
    assert(base.use_count() == 0);
    {
        using slot_type = decltype(&my_slot::onClick);
        using traits = traits_slot<slot_type>;
        using T0 = traits::args_tuple;
        static_assert(std::is_same_v<std::tuple<int>, T0>);

        using T1 = traits::ref_args_tuple;
        static_assert(std::is_same_v<std::tuple<int &>, T1>);

        using T2 = traits::full_ref_tuple;
        // NOTE: 完美
        static_assert(std::is_same_v<std::tuple<my_slot &, int &>, T2>);

        static_assert(not traits::sloter_type_is_pointer);

        auto impl = std::make_shared<
            slot_impl<slot_type, typename traits_slot<slot_type>::full_ref_tuple>>(
            &my_slot::onClick);
        slot_interface *fun = impl.get();
        fun->invoke(s, 2); // NOTE: 这里注意一下就行了
        assert(s.value == 2);

        // c0: 校验两边
        using TS = valid_signal_impl<signal_click>::args_tuple;
        using TS1 = traits_slot<slot_type>::args_tuple;
        static_assert(std::is_same_v<TS, TS1>); // NOTE: 就这样

        base = impl;
    }
    assert(base.use_count() == 1);
    fun = base.get();
    fun->invoke(s, 3);
    assert(s.value == 3); // NOTE: 完美屏蔽细节
}

struct connect_interface
{
    //-----------------------rcvr-----------------------------
    // slot_interface
    struct slot_type
    {
        void *recr;
        std::shared_ptr<slot_interface> slot;
    };
    void add_slot_impl(slot_type &&slot)
    {
        slot_impls.emplace_back(std::move(slot));
    }
    std::vector<slot_type> slot_impls;
    //--------------------------------------------------------

    //-----------------------sndr-----------------------------
    std::unordered_map<object_id, std::vector<std::weak_ptr<slot_interface>>>
        signal_slot_map;
    //--------------------------------------------------------

    constexpr auto &as_sndr() noexcept
    {
        return *this;
    }
    constexpr auto &as_rcvr() noexcept
    {
        return *this;
    }

    template <typename signal_key, std::derived_from<connect_interface> Rcvr,
              typename slot_type>
        requires(valid_slot<traits_slot<slot_type>> && valid_signal<signal_key> &&
                 signal_slot_match<signal_key, slot_type>)
    static auto connect(connect_interface *sndr, Rcvr *recr, slot_type slot)
    {
        auto s = std::make_shared<
            slot_impl<slot_type, typename traits_slot<slot_type>::full_ref_tuple>>(
            std::move(slot));

        static_cast<connect_interface *>(recr)->as_rcvr().add_slot_impl(
            {.recr = recr, .slot = s});

        object_id signal_id = object_id::make_signal_id<signal_key>();
        sndr->as_sndr().signal_slot_map[signal_id].emplace_back(
            std::weak_ptr<slot_interface>{s});
    }
    template <typename signal_key, typename... Args>
        requires(valid_signal_args<signal_key, Args...>)
    void emit(Args... args)
    {
    }
};

void test_weak_ptr()
{

    struct my_signal : connect_interface
    {
        using signal_click = void(int key);
        void submit_key_event(int v = 0)
        {
            emit<signal_click>(v);
        }
    };
    struct my_slot : connect_interface
    {

        using signal_click_callback = void();

        my_slot() noexcept {}
        void onClick(this my_slot &self, int key) noexcept
        {
            self.value = key;
            // self.emit<signal_click_callback>();
        }
        int value{-1};
    };

    static int construct_total = 0; // 总构造次数
    static int copy_total = 0;      // 总拷贝次数
    static int move_total = 0;      // 总移动次数
    static int destruct_total = 0;  // 总析构次数
    static int alive_count = 0;     // 当前存活对象数
    struct my_slot_impl
    {
        void operator()(my_slot *self, int key) noexcept
        {
            assert(self != nullptr);
            std::cout << "my_slot_impl: " << key << '\n';
            self->value = key;
        }

        // 默认构造函数
        my_slot_impl()
        {
            ++construct_total;
            ++alive_count;
            std::cout << "default constructor, alive: " << alive_count << '\n';
        }

        // 拷贝构造函数
        my_slot_impl(const my_slot_impl &)
        {
            ++construct_total;
            ++copy_total;
            ++alive_count;
            std::cout << "copy constructor, alive: " << alive_count
                      << " (total copies: " << copy_total << ")\n";
        }

        // 移动构造函数
        my_slot_impl(my_slot_impl &&) noexcept
        {
            ++construct_total;
            ++move_total;
            ++alive_count;
            std::cout << "move constructor, alive: " << alive_count
                      << " (total moves: " << move_total << ")\n";
        }

        // 拷贝赋值运算符
        my_slot_impl &operator=(const my_slot_impl &)
        {
            ++copy_total;
            std::cout << "copy assignment, alive: " << alive_count
                      << " (total copies: " << copy_total << ")\n";
            return *this;
        }

        // 移动赋值运算符
        my_slot_impl &operator=(my_slot_impl &&) noexcept
        {
            ++move_total;
            std::cout << "move assignment, alive: " << alive_count
                      << " (total moves: " << move_total << ")\n";
            return *this;
        }

        // 析构函数
        ~my_slot_impl() noexcept
        {
            ++destruct_total;
            --alive_count;
            std::cout << "destructor, alive: " << alive_count
                      << " (total destructs: " << destruct_total << ")\n";
        }

        // 打印统计信息
        static void print_stats()
        {
            std::cout << "\n=== FINAL STATISTICS ===\n"
                      << "Total constructions: " << construct_total << '\n'
                      << "Total copies: " << copy_total << '\n'
                      << "Total moves: " << move_total << '\n'
                      << "Total destructs: " << destruct_total << '\n'
                      << "Currently alive: " << alive_count << '\n';

            // 检查是否匹配
            bool balanced = (construct_total == destruct_total);
            std::cout << "Construction/destruction balanced: "
                      << (balanced ? "YES" : "NO") << '\n';
            std::cout << "==========================\n";
        }
    };

    // NOTE:1. 验证weak_ptr是否内存泄漏
    {
        my_signal signaler;
        my_slot sloter;
        connect_interface::connect<my_signal::signal_click>(&signaler, &sloter,
                                                            my_slot_impl{});

        std::shared_ptr<slot_interface> slot =
            signaler
                .signal_slot_map[object_id::make_signal_id<my_signal::signal_click>()][0]
                .lock();
        assert(slot);
        slot->invoke(sloter, 2);
        assert(sloter.value == 2);
        auto &ref = sloter;
        slot->invoke(ref, 3);
        assert(sloter.value == 3);
        auto *p = &sloter;
        slot->invoke(*p, 4);
        assert(sloter.value == 4);

        my_slot_impl::print_stats();
    }
    my_slot_impl::print_stats();

    // NOTE: 成员
    {
        my_signal signaler;
        my_slot sloter;
        connect_interface::connect<my_signal::signal_click>(&signaler, &sloter,
                                                            &my_slot::onClick);
        std::shared_ptr<slot_interface> slot =
            signaler
                .signal_slot_map[object_id::make_signal_id<my_signal::signal_click>()][0]
                .lock();
        slot->invoke(sloter, 2);
        assert(sloter.value == 2);
        auto &ref = sloter;
        slot->invoke(ref, 3);
        assert(sloter.value == 3);
        auto *p = &sloter;
        slot->invoke(*p, 4);
        assert(sloter.value == 4);
    }
    // NOTE: lambda
    {
        my_signal signaler;
        my_slot sloter;
        connect_interface::connect<my_signal::signal_click>(
            &signaler, &sloter,
            [](my_slot *self, int key) noexcept { self->value = key; });
        std::shared_ptr<slot_interface> slot =
            signaler
                .signal_slot_map[object_id::make_signal_id<my_signal::signal_click>()][0]
                .lock();
        slot->invoke(sloter, 2);
        assert(sloter.value == 2);
        auto &ref = sloter;
        slot->invoke(ref, 3);
        assert(sloter.value == 3);
        auto *p = &sloter;
        slot->invoke(*p, 4);
        assert(sloter.value == 4);
        // NOTE: 捕获
        {
            my_signal signaler;
            my_slot sloter;
            connect_interface::connect<my_signal::signal_click>(
                &signaler, &sloter, [&sloter](my_slot *self, int key) noexcept {
                    self->value = key;
                    assert(self == &sloter); // NOTE: 允许捕获
                });
            std::shared_ptr<slot_interface> slot =
                signaler
                    .signal_slot_map[object_id::make_signal_id<my_signal::signal_click>()]
                                    [0]
                    .lock();
            slot->invoke(sloter, 2);
            assert(sloter.value == 2);
            auto &ref = sloter;
            slot->invoke(ref, 3);
            assert(sloter.value == 3);
            auto *p = &sloter;
            slot->invoke(*p, 4);
            assert(sloter.value == 4);
        }
    }
}

int main()
{

    test_invoke();
    test_weak_ptr();
    std::cout << "main done\n";
    return 0;
}
// NOLINTEND