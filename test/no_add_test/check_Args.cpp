#include <iostream>
#include <type_traits>
#include <tuple>
#include <utility>

// 基础模板
template <typename... T>
auto check_trefref(T &&...t) -> std::tuple<T &&...>; // NOLINT

template <typename... T>
auto check_tref(T &&...t) -> std::tuple<T &...>;

template <typename... T>
auto check_t(T &&...t) -> std::tuple<T...>;

// 辅助类型检查
template <typename T>
struct TypeInfo;

void func(int) {} // NOLINT

struct my_class
{
    int value;
    std::string name;
};

// NOLINTBEGIN
int main()
{
    std::cout << "=== 1. 基本类型测试 ===\n";

    int x = 10;
    const int cx = 20;
    int &rx = x;
    const int &crx = cx;

    // 测试 check_trefref (完美转发)
    {
        // NOTE: check_trefref  -> T&& 要么是左值引用要么是右值，const 被转发
        //  c0: 一句话：参数是左值 ->T& ；move右值-> T&& ；立即值 -> T&&
        //  c0: const 被传递到 T&& 中

        using Type1 = decltype(check_trefref(x)); // T = int&, T&& = int&
        static_assert(std::is_same_v<Type1, std::tuple<int &>>,
                      "x -> T = int&, T&& = int& (引用折叠)");

        using Type2 = decltype(check_trefref(cx)); // T = const int&, T&& = const int&
        static_assert(std::is_same_v<Type2, std::tuple<const int &>>,
                      "cx -> T = const int&, T&& = const int&");

        using Type3 = decltype(check_trefref(rx)); // T = int&, T&& = int&
        static_assert(std::is_same_v<Type3, std::tuple<int &>>,
                      "rx -> T = int&, T&& = int&");

        using Type4 = decltype(check_trefref(crx)); // T = const int&, T&& = const int&
        static_assert(std::is_same_v<Type4, std::tuple<const int &>>,
                      "crx -> T = const int&, T&& = const int&");

        using Type5 = decltype(check_trefref(42)); // T = int, T&& = int&&
        static_assert(std::is_same_v<Type5, std::tuple<int &&>>,
                      "42 -> T = int, T&& = int&&");

        using Type6 = decltype(check_trefref(std::move(x))); // T = int, T&& = int&&
        static_assert(std::is_same_v<Type6, std::tuple<int &&>>,
                      "std::move(x) -> T = int, T&& = int&&");
    }

    // 测试 check_tref (左值引用)
    {
        // NOTE: check_trefref  -> T&& 要么是左值引用要么是右值，const 被转发
        // c1: 返回的一定是引用，const 被转发

        using Type1 = decltype(check_tref(x)); // T = int, T& = int&
        static_assert(std::is_same_v<Type1, std::tuple<int &>>,
                      "x -> T = int -> T& = int&");

        using Type2 = decltype(check_tref(cx)); // T = const int, T& = const int&
        static_assert(std::is_same_v<Type2, std::tuple<const int &>>,
                      "cx -> T = const int -> T& = const int&");

        // check_tref(42); // 编译错误: 不能绑定右值到左值引用

        using T3 = decltype(check_tref(42));
        static_assert(std::is_same_v<T3, std::tuple<int &>>,
                      "42 -> T = int -> T& =  int&");

        // C0: T&& 能接收 my_class{}，并不奇怪。 T&&参数就是匹配一起类型的
        using T4 = decltype(check_tref(my_class{}));
        static_assert(std::is_same_v<T4, std::tuple<my_class &>>,
                      "my_class{} -> T = my_class -> T& =  my_class &");
    }

    // 测试 check_t (值类型)
    {
        // NOTE: check_t：不要觉得 std::tuple<T...> 就一定不是引用类型了
        // c3: 立即值：T&& ->T . 否则 T&& ->T == T&& -> T&
        // c3: 换句话说：auto check_t(T &&...t) -> std::tuple<T...> 不要觉得。不会返回引用

        using Type1 = decltype(check_t(x)); // T = int
        static_assert(std::is_same_v<Type1, std::tuple<int &>>, "x -> T = int &");

        using Type2 = decltype(check_t(cx)); // T = const int
        static_assert(std::is_same_v<Type2, std::tuple<const int &>>,
                      "cx -> T = const int &");

        using Type3 = decltype(check_t(rx)); // T = int
        static_assert(std::is_same_v<Type3, std::tuple<int &>>,
                      "rx -> T = int & (引用被剥离)");

        using Type4 = decltype(check_t(42)); // T = int
        static_assert(std::is_same_v<Type4, std::tuple<int>>, "42 -> T = int");

        using T4 = decltype(check_t(my_class{}));
        static_assert(std::is_same_v<T4, std::tuple<my_class>>,
                      "my_class{} -> T = my_class -> T& =  my_class &");
    }

    std::cout << "=== 2. 普通用户类型测试 ===\n";

    my_class obj{};
    const my_class cobj{};
    const my_class &crobj = cobj;

    // 测试 check_trefref
    {
        // NOTE: auto check_trefref(T &&...t) -> std::tuple<T &&...>; 和普通类型一样
        // c4: 要么是左引用，要么右引用，const 转发

        using Type1 = decltype(check_trefref(obj)); // T = MyClass&, T&& = MyClass&
        static_assert(std::is_same_v<Type1, std::tuple<my_class &>>,
                      "obj -> T = MyClass&, T&& = MyClass&");

        using Type2 =
            decltype(check_trefref(cobj)); // T = const MyClass&, T&& = const MyClass&
        static_assert(std::is_same_v<Type2, std::tuple<const my_class &>>,
                      "cobj -> T = const MyClass&, T&& = const MyClass&");

        using Type3 =
            decltype(check_trefref(my_class{300})); // T = MyClass, T&& = MyClass&&
        static_assert(std::is_same_v<Type3, std::tuple<my_class &&>>,
                      "MyClass{} -> T = MyClass, T&& = MyClass&&");
    }

    // 测试 check_tref
    {
        // c5: 一样，一定返回引用：auto check_tref(T &&...t) -> std::tuple<T &...>;
        // c5: 只能是左引用
        using Type1 = decltype(check_tref(obj)); // T = MyClass, T& = MyClass&
        static_assert(std::is_same_v<Type1, std::tuple<my_class &>>,
                      "obj -> T = MyClass, T& = MyClass&");

        using Type4 = decltype(check_tref(std::move(obj)));
        static_assert(std::is_same_v<Type1, Type4>);

        using Type2 = decltype(check_tref(cobj));
        using Type3 = decltype(check_tref(crobj));

        static_assert(std::is_same_v<Type2, Type3>);

        static_assert(std::is_same_v<Type2, std::tuple<const my_class &>>,
                      "obj -> T =const MyClass, T& = MyClass&");

        using T4 = decltype(check_tref(my_class{}));
        static_assert(std::is_same_v<T4, std::tuple<my_class &>>,
                      "my_class{} -> T = my_class -> T& =  my_class &");
    }

    // 测试 check_t
    {
        // c6: auto check_t(T &&...t) -> std::tuple<T...>
        // c6: 引用还是引用。立即值 -> T . 右值 -> T
        using Type1 = decltype(check_t(obj)); // T = MyClass
        static_assert(std::is_same_v<Type1, std::tuple<my_class &>>,
                      "obj -> T = MyClass (引用被剥离)");

        using Type2 = decltype(check_t(cobj)); // T = const MyClass
        static_assert(std::is_same_v<Type2, std::tuple<const my_class &>>,
                      "cobj -> T = const MyClass");

        using T4 = decltype(check_t(my_class{}));
        static_assert(std::is_same_v<T4, std::tuple<my_class>>,
                      "my_class{} -> T = my_class -> T& =  my_class ");

        using Type4 = decltype(check_t(std::move(obj)));

        static_assert(std::is_same_v<Type4, T4>);
    }

    std::cout << "=== 3. 复杂类型测试 ===\n";

    // 测试指针类型
    int *ptr = &x;
    const int *cptr = &cx;
    int *const const_ptr = &x;

    {
        // c7: 一样
        using Type1 = decltype(check_trefref(ptr)); // T = int*&, T&& = int*&
        static_assert(std::is_same_v<Type1, std::tuple<int *&>>,
                      "ptr -> T = int*&, T&& = int*&");

        using Type2 = decltype(check_trefref(cptr)); // T = const int*&, T&& = const int*&
        static_assert(std::is_same_v<Type2, std::tuple<const int *&>>,
                      "cptr -> T = const int*&, T&& = const int*&");

        using Type3 =
            decltype(check_trefref(const_ptr)); // T = int* const&, T&& = int* const&
        static_assert(std::is_same_v<Type3, std::tuple<int *const &>>,
                      "const_ptr -> T = int* const&, T&& = int* const&");
    }

    // 测试数组类型
    int arr[3] = {1, 2, 3};
    const int carr[3] = {4, 5, 6};

    {
        // c8: 保持类型，一样
        using Type1 = decltype(check_trefref(arr)); // T = int(&)[3], T&& = int(&)[3]
        static_assert(std::is_same_v<Type1, std::tuple<int (&)[3]>>,
                      "arr -> T = int(&)[3], T&& = int(&)[3]");

        using Type2 =
            decltype(check_trefref(carr)); // T = const int(&)[3], T&& = const int(&)[3]
        static_assert(std::is_same_v<Type2, std::tuple<const int (&)[3]>>,
                      "carr -> T = const int(&)[3], T&& = const int(&)[3]");
    }

    // 测试函数指针类型
    using FuncPtr = void (*)(int);

    FuncPtr fptr = func;

    {
        // c9: 一样
        using Type1 =
            decltype(check_trefref(fptr)); // T = void(*&)(int), T&& = void(*&)(int)
        static_assert(std::is_same_v<Type1, std::tuple<FuncPtr &>>,
                      "fptr -> T = FuncPtr&, T&& = FuncPtr&");
        static_assert(std::is_same_v<Type1, std::tuple<void (*&)(int)>>,
                      "fptr -> T = FuncPtr&, T&& = FuncPtr&");

        // c9: 注意函数实例是 指针，函数类型和函数指针变量不同。
        using Type2 =
            decltype(check_trefref(func)); // T = void(&)(int), T&& = void(&)(int)
        static_assert(std::is_same_v<Type2, std::tuple<void (&)(int)>>,
                      "func -> T = void(&)(int), T&& = void(&)(int)");

        static_assert(not std::is_same_v<Type1, Type2>);
    }

    // 测试多参数
    {
        // c11: T&& 参数 -> tuple<T&&> 一定是引用
        using Type1 = decltype(check_trefref(x, 42, obj, std::move(obj)));
        // x: int&, 42: int&&, obj: MyClass&, std::move(obj): MyClass&&
        static_assert(
            std::is_same_v<Type1, std::tuple<int &, int &&, my_class &, my_class &&>>,
            "多参数完美转发");
    }

    std::cout << "所有静态断言测试通过！\n";
    std::cout << "main done\n";
    return 0;
}
// NOLINTEND