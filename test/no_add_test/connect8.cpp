#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <iostream>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <cstring>
#include <type_traits>
#include <vector>
#include <chrono>

// NOLINTBEGIN

template <typename T>
struct slot_function;

//---------------- pointer type -----------------------

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
    static constexpr size_t args_size = 0;
};

template <typename T>
concept valid_slot = requires { typename T::sloter_type; };
static_assert(valid_slot<slot_function<void (*)(int *, int) noexcept>>);    // ✅ true
static_assert(!valid_slot<slot_function<void (*)(int *, int &) noexcept>>); // ✅ false

//---------------- lambda type -----------------------
template <typename lambda, typename Sloter, no_cvref... Args>
    requires(row_pointer<Sloter> || row_lref<Sloter>)
struct slot_function<void (lambda ::*)(Sloter, Args...) noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename lambda, typename Sloter, no_cvref... Args>
    requires(row_pointer<Sloter> || row_lref<Sloter>)
struct slot_function<void (lambda ::*)(Sloter, Args...) const noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args &...>;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename lambda, typename Sloter>
    requires(row_pointer<Sloter> || row_lref<Sloter>)
struct slot_function<void (lambda ::*)(Sloter) noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<>;
    using ref_args_tuple = std::tuple<>;
    static constexpr size_t args_size = 0;
};

template <typename lambda, typename Sloter>
    requires(row_pointer<Sloter> || row_lref<Sloter>)
struct slot_function<void (lambda ::*)(Sloter) const noexcept>
{
    using sloter_type = Sloter;
    using args_tuple = std::tuple<>;
    using ref_args_tuple = std::tuple<>;
    static constexpr size_t args_size = 0;
};
// class member
template <typename Class, typename... Args>
struct slot_function<void (Class ::*)(Args...) const noexcept>
{
    using sloter_type = Class;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args...>;
    static constexpr size_t args_size = sizeof...(Args);
};
template <typename Class, typename... Args>
struct slot_function<void (Class ::*)(Args...) noexcept>
{
    using sloter_type = Class;
    using args_tuple = std::tuple<Args...>;
    using ref_args_tuple = std::tuple<Args...>;
    static constexpr size_t args_size = sizeof...(Args);
};

template <typename T>
    requires(
        std::is_object_v<T> &&
            requires() {
                typename slot_function<decltype(&T::operator())>::sloter_type;
            } ||
        (std::is_pointer_v<T> || std::is_member_function_pointer_v<T>) &&
            requires() { typename slot_function<T>::sloter_type; })
struct traits_slot
{
    using function_type = decltype([]() consteval {
        if constexpr (std::is_object_v<T> && requires() { &T::operator(); })
            return static_cast<decltype(&T::operator())>(nullptr);
        else
            return static_cast<T>(nullptr);
    }());
    static constexpr bool is_lambda = std::is_object_v<T>;
    using traits = slot_function<function_type>;
    using sloter_type = traits::sloter_type;
    using args_tuple = typename traits::args_tuple;
    using ref_args_tuple = typename traits::ref_args_tuple;
    static constexpr size_t args_size = traits::args_size;
    static constexpr bool sloter_type_is_pointer = std::is_pointer_v<sloter_type>;
};

// NOTE: 0. 存放对象id
struct object_id final
{
    constexpr object_id(const void *ptr) noexcept : value{uintptr_t(ptr)} {}
    constexpr object_id(size_t v) noexcept : value{v} {}

    bool operator==(const object_id &other) const noexcept = default;
    auto operator<=>(const object_id &b) const = default;

    template <typename signal_key>
    static constexpr object_id make_signal_id() noexcept
    {
        return object_id{typeid(signal_key).hash_code()};
    }
    friend struct std::hash<object_id>;

  private:
    size_t value;
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

struct slot_interface
{
    slot_interface() = default;
    slot_interface(const slot_interface &) = default;
    slot_interface(slot_interface &&) = default;
    slot_interface &operator=(const slot_interface &) = default;
    slot_interface &operator=(slot_interface &&) = default;
    constexpr virtual void invoke_impl(void *args) noexcept = 0;
    constexpr virtual ~slot_interface() noexcept = default;

    template <typename... Args>
    constexpr void invoke(Args &&...args) & noexcept
    {
        auto args_tuple = std::forward_as_tuple(std::forward<Args>(args)...);
        invoke_impl(&args_tuple);
    }
};

template <typename Rcvr, typename Impl>
struct slot_impl : slot_interface
{
    using traits = traits_slot<Impl>;
    using ref_args_tuple = traits::ref_args_tuple;
    static constexpr size_t args_size = traits::args_size;

    constexpr void invoke_impl(void *args) noexcept override
    {
        auto &tuple_args = *static_cast<ref_args_tuple *>(args);

        [&]<size_t... I>(std::index_sequence<I...>) noexcept {
            // 1. rcvr_ requires
            if constexpr (requires {
                              (slot_)(rcvr_, std::move(std::get<I>(tuple_args))...);
                          })
                (slot_)(rcvr_, std::move(std::get<I>(tuple_args))...);
            else if constexpr (requires {
                                   (slot_)(*rcvr_, std::move(std::get<I>(tuple_args))...);
                               })
                (slot_)(*rcvr_, std::move(std::get<I>(tuple_args))...);
            // 2. member_function
            else if constexpr (std::is_member_function_pointer_v<Impl> && requires {
                                   std::mem_fn(slot_)(
                                       rcvr_, std::move(std::get<I>(tuple_args))...);
                               })
                std::mem_fn(slot_)(rcvr_, std::move(std::get<I>(tuple_args))...);
            // 3. no rcvr_ requires
            else if constexpr (requires {
                                   (slot_)(std::move(std::get<I>(tuple_args))...);
                               })
                (slot_)(std::move(std::get<I>(tuple_args))...);
            else
            {
                static_assert(false, "Cannot invoke slot function");
            }
        }(std::make_index_sequence<args_size>{});
    }

    constexpr slot_impl(Rcvr *rcvr, Impl slot) noexcept
        : rcvr_{rcvr}, slot_{std::move(slot)}
    {
    }

#if defined(_MSC_VER)
#define MY_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif defined(__clang__) || defined(__GNUC__)
#define MY_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define MY_NO_UNIQUE_ADDRESS // 不支持该属性的编译器
#endif
  private:
    Rcvr *rcvr_;
    MY_NO_UNIQUE_ADDRESS Impl slot_;
#undef MY_NO_UNIQUE_ADDRESS
};

template <typename signal_click, typename slot_type>
concept signal_slot_match =
    std::same_as<typename valid_signal_impl<signal_click>::args_tuple,
                 typename traits_slot<slot_type>::args_tuple>;

struct connect_object
{
    struct connect_ptr
    {
        static constexpr int MAX_COUNT = 2;

        constexpr void release() noexcept
        {
            --ref_count_;
            if (ref_count_ == 1)
            {
                delete slot_;
                return;
            }
            delete this;
        }
        [[nodiscard]] constexpr bool rcvr_hold() const noexcept
        {
            return ref_count_ == MAX_COUNT;
        }

        constexpr connect_ptr(slot_interface *slot) noexcept : slot_{slot}
        {
            assert(slot_ != nullptr);
        }

        template <typename... Args>
        constexpr void invoke(Args &&...args) const noexcept
        {
            assert(rcvr_hold());
            slot_->invoke(std::forward<Args>(args)...);
        }
        connect_ptr(const connect_ptr &) = delete;
        connect_ptr &operator=(const connect_ptr &) = delete;
        connect_ptr(connect_ptr &&) = delete;
        connect_ptr &operator=(connect_ptr &&) = delete;

      private:
        int ref_count_{MAX_COUNT};
        slot_interface *slot_;
    };

    //--------------------------------rcvr--------------------------------------------
    constexpr bool connect_sndr(connect_ptr *ptr)
    {
        slots.emplace_back(ptr);
        return true;
    };
    constexpr void unsafe_remove_by_rcvr(connect_ptr *ptr)
    {
        std::erase_if(slots,
                      [&](connect_ptr *item) constexpr noexcept { return ptr == item; });
    }
    constexpr void as_rcvr_destroy() noexcept
    {
        for (connect_ptr *shared : slots)
            shared->release();
        slots.clear();
    }
    constexpr void disconnect_rcvr(connect_ptr *ptr)
    {
        assert(ptr->rcvr_hold());
        auto count = std::erase_if(
            slots, [&](connect_ptr *item) constexpr noexcept { return ptr == item; });
        assert(count == 1);
        ptr->release();
    }

    std::vector<connect_ptr *> slots;
    //--------------------------------rcvr end----------------------------------------

    //--------------------------------sndr--------------------------------------------

    constexpr bool connect_rcvr(object_id signal_key, connect_ptr *ptr)
    {
        signal_slot_map[signal_key].emplace_back(ptr);
        return true;
    };
    constexpr void unsafe_remove_by_sndr(object_id signal_key, connect_ptr *ptr)
    {
        std::erase_if(signal_slot_map[signal_key],
                      [&](connect_ptr *item) constexpr noexcept { return ptr == item; });
    }
    constexpr void as_sndr_destroy() noexcept
    {
        for (auto &[_, shareds] : signal_slot_map)
        {
            for (connect_ptr *shared : shareds)
                shared->release();
            shareds.clear();
        }
        signal_slot_map.clear();
    }
    constexpr void as_sndr_remove_expired_slot()
    {
        for (auto &[_, shareds] : signal_slot_map)
        {
            std::erase_if(shareds, [&](connect_ptr *shared) constexpr noexcept {
                if (not shared->rcvr_hold()) // find expired
                {
                    shared->release();
                    return true;
                }
                return false;
            });
        }
    }
    constexpr void disconnect_sndr(object_id signal_key, connect_ptr *ptr)
    {
        assert(not ptr->rcvr_hold());
        auto count = std::erase_if(
            signal_slot_map[signal_key],
            [&](connect_ptr *item) constexpr noexcept { return ptr == item; });
        assert(count == 1);
        ptr->release();
    }

    std::unordered_map<object_id, std::vector<connect_ptr *>> signal_slot_map;
    //--------------------------------sndr end----------------------------------------

    constexpr auto &as_sndr() noexcept
    {
        return *this;
    }
    constexpr auto &as_rcvr() noexcept
    {
        return *this;
    }

    connect_object() = default;
    ~connect_object() noexcept
    {
        as_rcvr_destroy();
        as_sndr_destroy();
    }
    connect_object(connect_object &&) = default;
    connect_object &operator=(connect_object &&) = default;

    connect_object(const connect_object &) = delete;
    connect_object &operator=(const connect_object &) = delete;

    //--------------------------------static function---------------------------
    template <typename signal_key, typename... Args>
        requires(valid_signal_args<signal_key, Args...>)
    void emit(Args... args)
    {
        auto it = signal_slot_map.find(object_id::make_signal_id<signal_key>());
        if (it != signal_slot_map.end())
        {
            bool has_expired{};
            for (connect_ptr *shared : it->second)
            {
                if (not shared->rcvr_hold())
                {
                    continue;
                    has_expired = true;
                }
                shared->invoke(std::move(args)...);
            }
            if (has_expired)
                as_sndr().as_sndr_remove_expired_slot();
        }
    }

    template <typename signal_key, std::derived_from<connect_object> Rcvr,
              typename slot_type>
        requires(valid_slot<traits_slot<slot_type>> && valid_signal<signal_key> &&
                 signal_slot_match<signal_key, slot_type>)
    static auto connect(connect_object *sndr, Rcvr *recr, slot_type slot) noexcept
        -> connect_ptr *
    {
        // NOTE: 多线程需要保持线程安全
        slot_interface *s =
            new (std::nothrow) slot_impl<Rcvr, slot_type>{recr, std::move(slot)};
        if (s == nullptr)
            return nullptr;

        connect_ptr *shared = new (std::nothrow) connect_ptr{s};
        if (shared == nullptr)
        {
            delete s;
            return nullptr;
        }

        try
        {
            static_cast<connect_object *>(recr)->as_rcvr().connect_sndr(shared);
            sndr->as_sndr().connect_rcvr(object_id::make_signal_id<signal_key>(), shared);
            return shared;
        }
        catch (...)
        {
            sndr->as_sndr().unsafe_remove_by_sndr(object_id::make_signal_id<signal_key>(),
                                                  shared);
            static_cast<connect_object *>(recr)->as_rcvr().unsafe_remove_by_rcvr(shared);
            delete s;
            delete shared;
            return nullptr;
        }
    }

    template <typename signal_key>
    static auto disconnect(connect_object *sndr, connect_object *recr, connect_ptr *slot)
    {
        assert(slot->rcvr_hold());
        recr->as_rcvr().disconnect_rcvr(slot);
        assert(not slot->rcvr_hold());
        sndr->as_sndr().disconnect_sndr(object_id::make_signal_id<signal_key>(), slot);
    }
};
void test_slot_ptr()
{
    struct my_signal : connect_object
    {
        using signal_click = void(int key);
        void submit_key_event(int v = 0)
        {
            this->emit<signal_click>(v);
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

        my_slot() noexcept {}
        void onClick(this my_slot &self, int key) noexcept
        {
            self.value = key;
            self.emit<signal_click_callback>();
        }
        int value{-1};
    };
    // c0: lambda
    {
        my_signal signaler;
        my_slot sloter;
        auto ret = connect_object::connect<my_signal::signal_click>(
            &signaler, &sloter, [&sloter](my_slot *self, int key) noexcept {
                self->value = key;
                assert(self == &sloter); // NOTE: 允许捕获
            });

        assert(ret);

        signaler.submit_key_event();
        assert(sloter.value == 0);
    }
    {
        my_signal signaler;
        my_slot sloter;
        auto ret = connect_object::connect<my_signal::signal_click>(
            &signaler, &sloter,
            [](my_slot *self, int key) noexcept { self->value = key; });

        assert(ret);

        signaler.submit_key_event(1);
        assert(sloter.value == 1);
    }
    // c0: member function
    {
        my_signal signaler;
        my_slot sloter;
        auto ret = connect_object::connect<my_signal::signal_click>(&signaler, &sloter,
                                                                    &my_slot::onClick);

        assert(ret);

        ret = connect_object::connect<my_slot::signal_click_callback>(
            &sloter, &signaler, &my_signal::onSuccessEmit);

        assert(ret);

        int listen_key;
        ret = connect_object::connect<my_signal::signal_click>(
            &signaler, &sloter,
            [&](const my_slot &, int key) noexcept { listen_key = key; });

        assert(not signaler.has_callback);
        signaler.submit_key_event(2); // c0: 一次发送
        assert(sloter.value == 2);
        assert(signaler.has_callback);

        assert(listen_key == 2); // c0: 所有slot 都受到信息
    }
    {
        my_signal signaler;
        my_slot sloter;
        auto ret = connect_object::connect<my_signal::signal_click>(&signaler, &sloter,
                                                                    &my_slot::onClick);

        assert(ret);

        ret = connect_object::connect<my_slot::signal_click_callback>(
            &sloter, &signaler, &my_signal::onSuccessEmit);

        assert(ret);

        int listen_key{-1}; // c0: 不需要 my_slot *,
        ret = connect_object::connect<my_signal::signal_click>(
            &signaler, &sloter, [&](int key) noexcept { listen_key = key; });

        assert(ret);
        assert(ret->rcvr_hold());

        std::cout << ">>> connect_object::disconnect start\n";
        connect_object::disconnect<my_signal::signal_click>(&signaler, &sloter, ret);
        std::cout << ">>> connect_object::disconnect end\n";
        // NOTE: dont`t use ret after disconnect

        assert(not signaler.has_callback);
        signaler.submit_key_event(2); // c0: 一次发送
        assert(sloter.value == 2);
        assert(signaler.has_callback);

        assert(listen_key == -1); // c0: 已经断开连接则不再得到更新
    }
    // NOTE: 总共8个 delete slot_， 即自动内存安全。自动删除，自动断开连接
    // NOTE: 8个 connect ，8 次  delete slot_
}

// 新增性能测试函数
void test_signal_slot_performance()
{
    std::cout << "\n=== 信号槽性能测试 ===\n";

    struct TestSignal : connect_object
    {
        using signal_test = void(int);
        using signal_empty = void();
    };

    struct TestSlot : connect_object
    {
        int value = 0;
        int empty_count = 0;

        // void onTest(this TestSlot &slef, int v) noexcept
        // {
        //     slef.value = v;
        // }
        void onTest(int v) noexcept
        {
            value = v;
        }
        void onTest2(int v) noexcept
        {
            value = v; // C0: class member
        }
        void onEmpty(this TestSlot &slef) noexcept
        {
            slef.empty_count++;
        }
    };
    {
        TestSlot s;
        auto v = &TestSlot::onTest2;
        TestSlot *ps = &s;
        // ps->(*v)(0);
        (ps->*v)(0);
        (s.*v)(0);
        std::mem_fn(v)(s, 2);
        std::mem_fn(v)(ps, 2);
        std::mem_fn (&TestSlot::onTest2)(s, 2);
        std::mem_fn (&TestSlot::onTest2)(ps, 2);
    }

    TestSignal signal;
    TestSlot slot;

    // 1. 测量直接函数调用开销（基准）
    {
        constexpr int ITERATIONS = 1000000;
        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < ITERATIONS; ++i)
        {
            slot.onTest(i);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "1. 直接函数调用: " << duration.count() / double(ITERATIONS)
                  << " ns/次\n";
    }

    // 2. 测量带参数信号槽调用开销
    {
        constexpr int ITERATIONS = 1000000;
        auto conn = connect_object::connect<TestSignal::signal_test>(&signal, &slot,
                                                                     &TestSlot::onTest);

        assert(conn);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < ITERATIONS; ++i)
        {
            signal.emit<TestSignal::signal_test>(i);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "2. 带参数信号槽: " << duration.count() / double(ITERATIONS)
                  << " ns/次\n";

        connect_object::disconnect<TestSignal::signal_test>(&signal, &slot, conn);
    }

    // 3. 测量无参数信号槽调用开销
    {
        constexpr int ITERATIONS = 1000000;
        auto conn = connect_object::connect<TestSignal::signal_empty>(&signal, &slot,
                                                                      &TestSlot::onEmpty);

        assert(conn);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < ITERATIONS; ++i)
        {
            signal.emit<TestSignal::signal_empty>();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "3. 无参数信号槽: " << duration.count() / double(ITERATIONS)
                  << " ns/次\n";
        std::cout << "   槽调用次数: " << slot.empty_count << "\n";

        connect_object::disconnect<TestSignal::signal_empty>(&signal, &slot, conn);
    }

    // 4. 测量lambda连接开销
    {
        constexpr int ITERATIONS = 1000000;
        int lambda_counter = 0;

        auto conn = connect_object::connect<TestSignal::signal_test>(
            &signal, &slot,
            [&lambda_counter](TestSlot *, int) noexcept { lambda_counter++; });

        assert(conn);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < ITERATIONS; ++i)
        {
            signal.emit<TestSignal::signal_test>(i);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "4. Lambda连接: " << duration.count() / double(ITERATIONS)
                  << " ns/次\n";
        std::cout << "   Lambda计数: " << lambda_counter << "\n";

        connect_object::disconnect<TestSignal::signal_test>(&signal, &slot, conn);
    }

    // 5. 测量多连接开销（1个信号，10个槽）
    {
        constexpr int ITERATIONS = 100000;
        constexpr int SLOT_COUNT = 10;

        std::vector<std::unique_ptr<TestSlot>> slots;
        std::vector<connect_object::connect_ptr *> connections;

        // 创建10个槽
        for (int i = 0; i < SLOT_COUNT; ++i)
        {
            auto slot_ptr = std::make_unique<TestSlot>();
            auto conn = connect_object::connect<TestSignal::signal_test>(
                &signal, slot_ptr.get(), &TestSlot::onTest);

            assert(conn);
            slots.push_back(std::move(slot_ptr));
            connections.push_back(conn);
        }

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < ITERATIONS; ++i)
        {
            signal.emit<TestSignal::signal_test>(i);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "5. 多连接(" << SLOT_COUNT
                  << "槽): " << duration.count() / double(ITERATIONS) << " ns/次发射\n";
        std::cout << "   每个槽平均: "
                  << duration.count() / double(ITERATIONS * SLOT_COUNT) << " ns/槽\n";

        // 断开连接
        for (size_t i = 0; i < connections.size(); ++i)
        {
            connect_object::disconnect<TestSignal::signal_test>(&signal, slots[i].get(),
                                                                connections[i]);
        }
    }

    // 6. 测量连接/断开开销
    {
        constexpr int ITERATIONS = 10000;

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < ITERATIONS; ++i)
        {
            TestSlot temp_slot;
            auto conn = connect_object::connect<TestSignal::signal_test>(
                &signal, &temp_slot, &TestSlot::onTest);

            assert(conn);

            // 立即断开
            connect_object::disconnect<TestSignal::signal_test>(&signal, &temp_slot,
                                                                conn);
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "6. 连接+断开开销: " << duration.count() / double(ITERATIONS)
                  << " ns/次\n";
    }

    std::cout << "\n=== 性能测试完成 ===\n\n";
}

// 更详细的对比测试：与std::function对比
void test_vs_std_function()
{
    std::cout << "\n=== 与std::function性能对比 ===\n";

    struct TestObj
    {
        int value = 0;
        void method(int v) noexcept
        {
            value = v;
        }
    };

    constexpr int ITERATIONS = 1000000;
    TestObj obj;

    // 1. std::function调用
    {
        std::function<void(int)> func = [&obj](int v) {
            obj.value = v;
        };

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i)
        {
            func(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "std::function调用: " << duration.count() / double(ITERATIONS)
                  << " ns/次\n";
    }

    // 2. 原始函数指针调用
    {
        void (*func)(TestObj *, int) = [](TestObj *o, int v) {
            o->value = v;
        };

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i)
        {
            func(&obj, i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "原始函数指针: " << duration.count() / double(ITERATIONS)
                  << " ns/次\n";
    }

    // 3. 你的信号槽
    {
        struct TestSignal : connect_object
        {
            using signal_test = void(int);
        };
        struct TestSlot : connect_object
        {
            int value = 0;
            void onTest(this TestSlot &slef, int v) noexcept
            {
                slef.value = v;
            }
        };

        TestSignal signal;
        TestSlot slot;

        auto conn = connect_object::connect<TestSignal::signal_test>(&signal, &slot,
                                                                     &TestSlot::onTest);

        assert(conn);

        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < ITERATIONS; ++i)
        {
            signal.emit<TestSignal::signal_test>(i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        std::cout << "你的信号槽: " << duration.count() / double(ITERATIONS)
                  << " ns/次\n";

        connect_object::disconnect<TestSignal::signal_test>(&signal, &slot, conn);
    }
    std::cout << "obj.value: " << obj.value;
    std::cout << "\n=== 对比测试完成 ===\n\n";
}

// 编译时开销测试
void test_compile_time_overhead()
{
    std::cout << "\n=== 编译时开销测试 ===\n";

    // 测试编译时类型检查的开销
    struct TestSignal : connect_object
    {
        using signal_one = void(int);
        using signal_two = void(int, double);
        using signal_three = void(int, double, const char *);
    };

    struct TestSlot : connect_object
    {
        void slot_one(this TestSlot &slef, int) noexcept {}
        void slot_two(this TestSlot &slef, int, double) noexcept {}
        void slot_three(this TestSlot &slef, int, double, const char *) noexcept {}
    };

    TestSignal signal;
    TestSlot slot;

    std::cout << "编译时类型检查测试（如果编译通过即表示正常）:\n";

    // 这些应该能编译通过
    auto conn1 = connect_object::connect<TestSignal::signal_one>(&signal, &slot,
                                                                 &TestSlot::slot_one);
    std::cout << "1. 单参数连接: " << (conn1 ? "成功" : "失败") << "\n";

    auto conn2 = connect_object::connect<TestSignal::signal_two>(&signal, &slot,
                                                                 &TestSlot::slot_two);
    std::cout << "2. 双参数连接: " << (conn2 ? "成功" : "失败") << "\n";

    auto conn3 = connect_object::connect<TestSignal::signal_three>(&signal, &slot,
                                                                   &TestSlot::slot_three);
    std::cout << "3. 三参数连接: " << (conn3 ? "成功" : "失败") << "\n";

    // 尝试错误连接（应该编译失败或返回nullptr）
    // 注：取消下面代码的注释会触发编译错误，这是正确的

    // 参数类型不匹配
    // auto conn_err1 = connect_object::connect<TestSignal::signal_one>(
    //     &signal, &slot, &TestSlot::slot_two); // 应该编译失败

    // 参数数量不匹配
    // auto conn_err2 = connect_object::connect<TestSignal::signal_two>(
    //     &signal, &slot, &TestSlot::slot_one); // 应该编译失败

    if (conn1)
        connect_object::disconnect<TestSignal::signal_one>(&signal, &slot, conn1);
    if (conn2)
        connect_object::disconnect<TestSignal::signal_two>(&signal, &slot, conn2);
    if (conn3)
        connect_object::disconnect<TestSignal::signal_three>(&signal, &slot, conn3);

    std::cout << "\n编译时检查工作正常！\n";
}

// 内存开销测试
void test_memory_overhead()
{
    std::cout << "\n=== 内存开销测试 ===\n";

    struct EmptySignal : connect_object
    {
        using signal_test = void();
    };
    struct EmptySlot : connect_object
    {
    };

    EmptySignal signal;
    EmptySlot slot;

    // 测量一个连接的内存开销
    size_t memory_before = 0;
    size_t memory_after = 0;

    // 简单估算：创建一个连接，看增加多少
    std::cout << "估算单个连接的内存开销:\n";

    // 连接前
    auto conn = connect_object::connect<EmptySignal::signal_test>(
        &signal, &slot, [](EmptySlot *) noexcept {});

    if (conn)
    {
        std::cout << "1. slot_impl大小: "
                  << sizeof(slot_impl<EmptySlot, decltype([](EmptySlot *) noexcept {})>)
                  << " 字节\n";

        std::cout << "2. connect_ptr大小: " << sizeof(connect_object::connect_ptr)
                  << " 字节\n";

        std::cout << "3. 总预估开销: "
                  << (sizeof(
                          slot_impl<EmptySlot, decltype([](EmptySlot *) noexcept {})>) +
                      sizeof(connect_object::connect_ptr))
                  << " 字节\n";

        connect_object::disconnect<EmptySignal::signal_test>(&signal, &slot, conn);
    }

    std::cout << "\n内存开销测试完成\n";
}

int main()
{
    test_slot_ptr();

    // 新增性能测试
    std::cout << "\n=======================\n";
    std::cout << "开始性能测试\n";
    std::cout << "=======================\n";

    test_signal_slot_performance();
    test_vs_std_function();
    test_compile_time_overhead();
    test_memory_overhead();
    std::cout << "main done\n";
    return 0;
}
// NOTE: 手写虚函数不会比 编译器的快。
//  NOLINTEND