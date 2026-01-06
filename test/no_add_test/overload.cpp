#include <bit>
#include <cstddef>
#include <iostream>
#include <cassert>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include <cstring>
#include <functional>

class MyClass
{
    int index_ = -1;

  public:
    void valueChanged() noexcept // NOLINT
    {
        std::cout << "valueChanged() called..................\n";
    }
    void valueChanged(int /*unused*/)
    {
        index_ = 1;
        std::cout << "valueChanged(int) called, index set to 1\n";
    }

    void valueChanged(double /*unused*/)
    {
        index_ = 2;
        std::cout << "valueChanged(double) called, index set to 2\n";
    }

    void valueChanged(int /*unused*/, const std::string & /*unused*/)
    {
        index_ = 3;
        std::cout << "valueChanged(int, string) called, index set to 3\n";
    }

    void valueUpdate(int /*unused*/) // NOLINT
    {
        std::cout << "valueUpdate() called..................\n";
    }

    static void staticValueUpdate(int /*unused*/)
    {
        std::cout << "staticValueUpdate(int) called..................\n";
    }

    [[nodiscard]] int index() const noexcept
    {
        return index_;
    }

    void reset()
    {
        index_ = -1;
        std::cout << "Index reset to -1\n";
    }
};

// NOTE:  noexcept 也要考虑到
struct class_b
{
    int index = -1;                // NOLINT
    void onValueChanged() noexcept // NOLINT
    {
        std::cout << "onValueChanged() called..................\n";
        index = 0;
    }
    void onValueChanged(int /*unused*/) // NOLINT
    {
        std::cout << "onValueChanged(int ) called..................\n";
        index = 1;
    }

    void singel() {}
};

consteval auto overload_test(void (MyClass::*ptr)(int)) -> void (MyClass::*)(int)
{
    return ptr;
}

// template <typename T>
// consteval auto overload(void (T::*ptr)()) -> void (T::*)()
// {
//     return ptr;
// }

template <typename T>
struct connect_info;
template <typename R, typename C, typename... param>
struct connect_info<R (C::*)(param...)>
{
    using type = R (*)(param...);
    using class_type = C;
};
template <typename R, typename C, typename... param>
struct connect_info<R (C::*)(param...) noexcept>
{
    using type = R (*)(param...) noexcept;
    using class_type = C;
};

template <typename R, typename C, typename... param>
struct connect_info<R (C::*const)(param...)>
{
    using type = R (*)(param...);
    using class_type = C;
};
template <typename R, typename C, typename... param>
struct connect_info<R (C::*const)(param...) noexcept>
{
    using type = R (*)(param...) noexcept;
    using class_type = C;
};

template <typename class_type, typename Fun, typename... Args>
constexpr void slot_invoke(class_type *ptr, Fun fun, Args &&...args) noexcept // NOLINT
{
    (ptr->*fun)(std::forward<Args>(args)...);
}

//---------
// 基础模板
template <typename... Params>
struct overload_t
{
    template <typename T, typename Ret>
    consteval auto operator()(Ret (T::*ptr)(Params...)) const noexcept
    {
        return ptr;
    }

    template <typename T, typename Ret>
    consteval auto operator()(Ret (T::*ptr)(Params...) noexcept) const noexcept
    {
        return ptr;
    }

    template <typename T, typename Ret>
    consteval auto operator()(Ret (T::*ptr)(Params...) const) const noexcept
    {
        return ptr;
    }

    template <typename T, typename Ret>
    consteval auto operator()(Ret (T::*ptr)(Params...) const noexcept) const noexcept
    {
        return ptr;
    }

    // deduce this
    template <typename Ret>
    consteval auto operator()(Ret (*ptr)(Params...)) const noexcept
    {
        return ptr;
    }

    template <typename Ret>
    consteval auto operator()(Ret (*ptr)(Params...) noexcept) const noexcept
    {
        return ptr;
    }
    template <typename>
    consteval auto operator()(void (*ptr)(Params...)) const noexcept
    {
        return ptr;
    }

    template <typename>
    consteval auto operator()(void (*ptr)(Params...) noexcept) const noexcept
    {
        return ptr;
    }
};

// 无参版本的特化
template <>
struct overload_t<>
{
    template <typename T, typename Ret>
    consteval auto operator()(Ret (T::*ptr)()) const noexcept
    {
        return ptr;
    }

    template <typename T, typename Ret>
    consteval auto operator()(Ret (T::*ptr)() noexcept) const noexcept
    {
        return ptr;
    }

    template <typename T, typename Ret>
    consteval auto operator()(Ret (T::*ptr)() const) const noexcept
    {
        return ptr;
    }

    template <typename T, typename Ret>
    consteval auto operator()(Ret (T::*ptr)() const noexcept) const noexcept
    {
        return ptr;
    }

    // deduce this
    template <typename Ret>
    consteval auto operator()(Ret (*ptr)()) const noexcept
    {
        return ptr;
    }

    template <typename Ret>
    consteval auto operator()(Ret (*ptr)() noexcept) const noexcept
    {
        return ptr;
    }

    template <typename>
    consteval auto operator()(void (*ptr)()) const noexcept
    {
        return ptr;
    }

    template <typename>
    consteval auto operator()(void (*ptr)() noexcept) const noexcept
    {
        return ptr;
    }
};

template <class... T>
inline constexpr overload_t<T...> overload{}; // NOLINT
//-------------

template <typename class_type, typename Fun>
struct slot_object
{
    class_type *obj;
    Fun slot;
    //
    template <typename... Args>
    void operator()(Args &&...args) const noexcept
    {
        (obj->*slot)(std::forward<Args>(args)...);
    }
};

struct slot_interface
{
    using store_type = void *;

    template <typename class_type, typename R, typename slot, typename... Args>
    constexpr slot_interface(class_type *obj, R (*fun)(slot, Args...)) noexcept
        : obj_ptr{std::bit_cast<store_type>(obj)}, fun_ptr{std::bit_cast<store_type>(fun)}
    {
        static_assert(
            std::is_same_v<std::remove_cvref_t<class_type>, std::remove_cvref_t<slot>>);
    }

    store_type obj_ptr; // NOLINT
    store_type fun_ptr; // NOLINT
};

// NOLINTBEGIN
int main()
{
    {
        constexpr auto func1 = overload_test(&MyClass::valueChanged); // OK的
        constexpr auto func2 = overload<int>(&MyClass::valueChanged);
        static_assert(std::is_same_v<decltype(func2), decltype(func1)>);

        constexpr auto func3 = overload<>(&MyClass::valueChanged);
        static_assert(
            std::is_same_v<connect_info<decltype(func3)>::type, void (*)() noexcept>);
    }
    // 需要指定确切的函数类型
    // constexpr auto func0 = &MyClass::valueChanged; //NOTE: 无法区分
    constexpr auto func1 = static_cast<void (MyClass::*)(int)>(&MyClass::valueChanged);
    constexpr auto func2 = static_cast<void (MyClass::*)(double)>(&MyClass::valueChanged);
    constexpr auto func3 =
        static_cast<void (MyClass::*)(int, const std::string &)>(&MyClass::valueChanged);

    constexpr auto func4 = &MyClass::valueUpdate; // NOTE: 可以区分

    auto func5 = &MyClass::valueUpdate;
    auto func6 = &MyClass::staticValueUpdate;

    using T = decltype(func4);

    static_assert(not std::is_same_v<decltype(func1), decltype(func2)>);
    static_assert(not std::is_same_v<decltype(func1), decltype(func3)>);
    static_assert(not std::is_same_v<decltype(func2), decltype(func3)>);

    static_assert(std::is_same_v<void (MyClass::*const)(int), T>, "must equal.");
    // NOTE:  (MyClass::*) 修饰
    static_assert(std::is_same_v<void (MyClass::*)(int), decltype(func5)>, "must equal.");

    // NOTE: 删掉 MyClass:: 即可
    static_assert(std::is_same_v<void (*)(int), decltype(func6)>, "must equal.");
    {
        using T0 = std::remove_pointer_t<decltype(func5)>;
        using T0 [[maybe_unused]] = std::remove_extent_t<decltype(func5)>; // NOTE: 无效

        // NOTE: ok. signal + slot 肯定可以安全校验了
        using T1 = connect_info<decltype(func5)>::type;
        static_assert(std::is_same_v<T1, decltype(func6)>);

        constexpr auto signal0 = overload<>(&MyClass::valueChanged);
        constexpr auto signal1 = overload<int>(&MyClass::valueChanged);

        constexpr auto slot0 = overload<>(&class_b::onValueChanged);
        constexpr auto slot1 = overload<int>(&class_b::onValueChanged);

        static_assert(std::is_same_v<typename connect_info<decltype(signal0)>::type,
                                     typename connect_info<decltype(slot0)>::type>);
        static_assert(std::is_same_v<typename connect_info<decltype(signal1)>::type,
                                     typename connect_info<decltype(slot1)>::type>);

        static_assert(std::is_same_v<typename connect_info<decltype(signal1)>::type,
                                     typename connect_info<decltype(overload<int>(
                                         &class_b::onValueChanged))>::type>);

        static_assert(not std::is_same_v<typename connect_info<decltype(signal1)>::type,
                                         typename connect_info<decltype(slot0)>::type>);

        // NOTE: 是否能归一化，值储存。 比如存放为 void * 。这样我们就能 connect_invoke
        // 直接调用
        class_b b;
        void *ptr_b = &b;
        slot_invoke(static_cast<class_b *>(ptr_b), slot0);
        assert(b.index == 0);

        slot_invoke(static_cast<class_b *>(ptr_b), slot1, 1);
        assert(b.index == 1);

        // NOTE: 我们只需要 保存 ： ptr_b 和  slot0。 可以直接封装到一个对象中

        using class_type = class_b;
        using slot_type = decltype(overload<>(&class_b::onValueChanged));
        // auto invoke = []<typename... Args>(void *ptr, void *fun, Args &&...args) {
        //     // (static_cast<class_type *>(ptr)->*(slot_type)(fun))(
        //     //     std::forward<Args>(args)...);
        // };
        // 调用时需要把函数指针转为 void*
        // invoke(ptr_b, (void *)(overload<>(&class_b::onValueChanged)));

        // NOTE: 使用 memcpy 绕过 不能将 成员指针 转化为 void*
        // //  1. 修改 invoke lambda，用 union 或 memcpy 安全转换
        // auto invoke = []<typename... Args>(void *ptr, void *fun, Args &&...args) {
        //     // 使用 memcpy 安全转换
        //     slot_type fun_ptr;
        //     std::memcpy(&fun_ptr, &fun, sizeof(slot_type)); // NOLINT
        //     (static_cast<class_type *>(ptr)->*fun_ptr)(std::forward<Args>(args)...);
        // };
        // // 2. 调用时用临时变量
        // void *slot_ptr;
        // std::memcpy(&slot_ptr, &slot0, sizeof(void *));
        // invoke(ptr_b, slot_ptr);

        // assert(b.index == 0); //NOTE: 崩溃。看来不让操作是有原因的

        slot_object<class_type, slot_type> inv{&b, slot0};
        inv();
        assert(b.index == 0);

        // c0: 编译器的结果是不一样的------ 所以需要注释。 mingw 不是标准
        //  static_assert(sizeof(slot0) == 2 * sizeof(void *)); // NOTE:
        //  这就是原因，为何？
        //  static_assert(sizeof(overload<int>(&class_b::onValueChanged)) ==
        //                2 * sizeof(void *));

        // static_assert(sizeof(overload<>(&class_b::singel)) == 2 * sizeof(void *));

        struct b
        {
            void singel() {}
        };
        // static_assert(sizeof(overload<>(&b::singel)) == 2 * sizeof(void *));

        // // NOTE: 因此这里就很关键了。
        // static_assert(sizeof(slot_object<class_type, slot_type>) == 3 * sizeof(void
        // *));

        struct A
        {
            int index = -1;
            void onValueChanged(this A &self, int b)
            {
                self.index = 0;
            }
            void onValueChanged(this A &self)
            {
                self.index = 1;
            }

            void bar(this A &self)
            {
                self.index = 2;
            }
        };
        auto p = &A::bar;
        static_assert(sizeof(p) == 1 * sizeof(void *)); // NOTE: 尼玛的
        // constexpr auto k = overload<int>(&A::onValueChanged); //NOTE: 怎么样都不行
        // &A::onValueChanged; //NOTE: 不行
        //
        {
            using impl_type = decltype(overload<A &, int>(&A::onValueChanged));
            using store_type = void *;

            // 0. 输入
            A a;
            impl_type impl_fun = overload<A &, int>(&A::onValueChanged);

            // 1. 存储（去掉const，避免后续多一层转换）
            store_type pfun_ptr =
                std::bit_cast<void *>(impl_fun); // NOTE: 美好，比static_cast 强
            store_type pobj_ptr = &a;

            // 断言
            assert(pfun_ptr == impl_fun);
            assert(pobj_ptr == &a);

            // 2. 调用测试
            (std::bit_cast<impl_type>(pfun_ptr))(*static_cast<A *>(pobj_ptr), 1);
            assert(a.index == 0);

            // NOTE: 统一抽象的实例 .  但是还是建议 const
            pfun_ptr = std::bit_cast<void *>(overload<A &>(&A::onValueChanged));
            (std::bit_cast<decltype(overload<A &>(&A::onValueChanged))>(pfun_ptr))(
                *static_cast<A *>(pobj_ptr));
            assert(a.index == 1);

            // constexpr //NOTE: bit_cast 优先。缺少更好的哦
            {
                constexpr auto impl_fun = overload<A &, int>(&A::onValueChanged);
                (std::bit_cast<impl_type>(impl_fun))(*static_cast<A *>(pobj_ptr), 1);
                assert(a.index == 0);
            }

            // NOTE: 使用 slot_interface
            {
                // 0. 输入
                A a;
                slot_interface slot{&a, overload<A &, int>(&A::onValueChanged)};

                // NOTE: 由 emit 调用，传递参数 int{1}
                (std::bit_cast<impl_type>(slot.fun_ptr))(*static_cast<A *>(slot.obj_ptr),
                                                         1);
                assert(a.index == 0);

                slot_interface slot2{&a, overload<A &>(&A::onValueChanged)};
                (std::bit_cast<decltype(overload<A &>(&A::onValueChanged))>(
                    slot2.fun_ptr))(*static_cast<A *>(slot2.obj_ptr));
                assert(a.index == 1);

                // NOTE: emit resultReady("处理结果"); // resultReady 是签名
            }
        }

        // NOTE: 正清本源。 还是 static_cast 好一些哦.....
        auto p1 = static_cast<void (*)(A &, int)>(&A::onValueChanged); // 带int参数的版本
        auto p2 = static_cast<void (*)(A &)>(&A::onValueChanged);      // 无参版本

        static_assert(sizeof(p1) == 1 * sizeof(void *));
        static_assert(sizeof(p2) == 1 * sizeof(void *));

        // NOTE: 因此。23后统统是
        A a;
        void *pa = &a;
        p(*static_cast<A *>(pa));

        assert(a.index == 2);

        p1(*static_cast<A *>(pa), 1);
        assert(a.index == 0);

        p2(*static_cast<A *>(pa));
        assert(a.index == 1);

        { // NOTE: 差别很大了。
            struct Y
            {
                int f(int, int) const &
                {
                    return 0;
                }
                int g(this Y const &, int, int)
                {
                    return 0;
                }

                // NOTE: 注意
                //   void g(int, int) const &; // Error: already declared
            };
            Y y;

            auto pf = &Y::f;
            // pf(y, 1, 2);   // error: pointers to member functions are not callable
            (y.*pf)(1, 2);            // ok
            std::invoke(pf, y, 1, 2); // ok

            auto pg = &Y::g;
            pg(y, 3, 4); // ok
            // (y.*pg)(3, 4);            // error: “pg” is not a pointer to member
            // function
            std::invoke(pg, y, 3, 4); // ok
        }
    }

    MyClass obj;

    {
        func6(1); // NOTE: static 函数 就这么简单

        (obj.*func5)(1); // 需要括号
    }

    // 测试1: 调用第一个重载函数
    std::cout << "\n=== Testing func1 (int) ===\n";
    assert(obj.index() == -1);
    (obj.*func1)(42); // 使用成员函数指针调用
    assert(obj.index() == 1);

    // 测试2: 调用第二个重载函数
    obj.reset();
    std::cout << "\n=== Testing func2 (double) ===\n";
    (obj.*func2)(3.14);
    assert(obj.index() == 2);

    // 测试3: 调用第三个重载函数
    obj.reset();
    std::cout << "\n=== Testing func3 (int, string) ===\n";
    (obj.*func3)(100, "test");
    assert(obj.index() == 3);

    // 测试4: 直接调用对比
    obj.reset();
    std::cout << "\n=== Direct calls for comparison ===\n";
    obj.valueChanged(10); // 调用int版本
    assert(obj.index() == 1);

    obj.valueChanged(20.0); // 调用double版本
    assert(obj.index() == 2);

    obj.valueChanged(30, "direct"); // 调用int,string版本
    assert(obj.index() == 3);

    // 测试5: 使用函数指针调用不同对象
    std::cout << "\n=== Testing with different objects ===\n";
    MyClass obj1, obj2;
    (obj1.*func1)(1);
    (obj2.*func2)(2.0);
    assert(obj1.index() == 1);
    assert(obj2.index() == 2);

    // 测试6: 验证不同函数指针的地址
    std::cout << "\n=== Function pointer addresses ===\n";
    std::cout << "func1 address: " << &func1 << '\n';
    std::cout << "func2 address: " << &func2 << '\n';
    std::cout << "func3 address: " << &func3 << '\n';

    // 验证它们确实是不同的指针
    assert((void *)&func1 != &func2 && (void *)&func1 != &func3 &&
           (void *)&func2 != &func3);

    // 测试7: 通过指针调用并验证参数传递
    std::cout << "\n=== Testing parameter passing ===\n";
    obj.reset();

    int int_param = 999;
    double double_param = 123.456;
    std::string str_param = "hello";

    (obj.*func1)(int_param);
    assert(obj.index() == 1);

    obj.reset();
    (obj.*func2)(double_param);
    assert(obj.index() == 2);

    obj.reset();
    (obj.*func3)(int_param, str_param);
    assert(obj.index() == 3);

    // 测试8: 使用函数指针数组
    std::cout << "\n=== Testing function pointer array ===\n";
    using MemberFunc = void (MyClass::*)(int);
    using MemberFuncDouble [[maybe_unused]] = void (MyClass::*)(double);
    using MemberFuncStr [[maybe_unused]] = void (MyClass::*)(int, const std::string &);

    // C++17 可以使用std::variant存储不同类型的函数指针
    // 这里简化处理，分别测试

    obj.reset();
    MemberFunc func_arr[] = {func1};
    (obj.*func_arr[0])(777);
    assert(obj.index() == 1);

    // 测试9: 验证static_cast的正确性
    std::cout << "\n=== Verifying static_cast correctness ===\n";
    obj.reset();

    // 错误转换应该导致编译错误，这里注释掉
    // auto wrong_func = static_cast<void (MyClass::*)(float)>(&MyClass::valueChanged); //
    // 编译错误

    // 正确转换应该可以工作
    auto same_as_func1 = static_cast<void (MyClass::*)(int)>(&MyClass::valueChanged);
    (obj.*same_as_func1)(888);
    assert(obj.index() == 1);
    assert(func1 == same_as_func1);

    std::cout << "\n=== All tests passed! ===\n";
    std::cout << "main done\n";

    return 0;
}
// NOLINTEND