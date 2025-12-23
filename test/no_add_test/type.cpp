#include <cassert>
#include <cstring>
#include <iostream>
#include <tuple>

// NOLINTBEGIN
enum class type
{
    INT,
    DOUBLE
};
struct a_class
{
    int a;
    type t;

    // a_class() : a{} {} //NOTE: 和聚合初始化是冲突的
};

// C 风格结构体
struct SimpleCStruct
{
    int a;
    double b;
    char c;
};

struct NestedCStruct
{
    int x;
    SimpleCStruct inner;
    float y;
};

struct WithArrays
{
    int arr[5];
    char str[10];
    double matrix[2][3];
};

struct WithPointers
{
    int *ptr;
    const char *str;
};

struct MixedData
{
    short s;
    long l;
    float f;
    double d;
    char ch;
};

// 包含位域的结构体
struct WithBitfields
{
    unsigned int flag : 1;
    unsigned int value : 4;
    unsigned int : 3; // 未命名位域
    int regular_int;
};

// 复杂嵌套结构体
struct ComplexNested
{
    SimpleCStruct simple;
    WithArrays arrays;
    MixedData mixed;
    NestedCStruct nested;
};

// 测试函数声明
void test_simple_c_struct();
void test_nested_c_struct();
void test_arrays();
void test_pointers();
void test_mixed_data();
void test_bitfields();
void test_complex_nested();
void test_partial_initialization();

int main()
{
    std::cout << "Testing C++23 {} initialization for C structs...\n\n";

    test_simple_c_struct();
    test_nested_c_struct();
    test_arrays();
    test_pointers();
    test_mixed_data();
    test_bitfields();
    test_complex_nested();
    test_partial_initialization();

    std::cout << "All tests passed! ✅\n";
    return 0;
}

void test_simple_c_struct()
{
    std::cout << "Testing SimpleCStruct...\n";

    SimpleCStruct obj{};

    // 所有基本类型成员应该被初始化为 0
    assert(obj.a == 0);
    assert(obj.b == 0.0);
    assert(obj.c == 0);

    // 验证内存确实被清零
    SimpleCStruct raw;
    std::memset(&raw, 0xCC, sizeof(raw)); // 填充垃圾数据
    raw = SimpleCStruct{};                // 使用 {} 初始化

    assert(raw.a == 0);
    assert(raw.b == 0.0);
    assert(raw.c == 0);

    std::cout << "  ✓ SimpleCStruct fully zero-initialized\n";
}

void test_nested_c_struct()
{
    std::cout << "Testing NestedCStruct...\n";

    NestedCStruct obj{};

    // 顶层成员
    assert(obj.x == 0);
    assert(obj.y == 0.0f);

    // 嵌套结构体成员
    assert(obj.inner.a == 0);
    assert(obj.inner.b == 0.0);
    assert(obj.inner.c == 0);

    std::cout << "  ✓ NestedCStruct recursively zero-initialized\n";
}

void test_arrays()
{
    std::cout << "Testing WithArrays...\n";

    WithArrays obj{};

    // 一维数组
    for (int i = 0; i < 5; ++i)
    {
        assert(obj.arr[i] == 0);
    }

    // 字符数组
    for (int i = 0; i < 10; ++i)
    {
        assert(obj.str[i] == 0);
    }

    // 二维数组
    for (int i = 0; i < 2; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            assert(obj.matrix[i][j] == 0.0);
        }
    }

    std::cout << "  ✓ Arrays fully zero-initialized\n";
}

void test_pointers()
{
    std::cout << "Testing WithPointers...\n";

    WithPointers obj{};

    // 指针应该被初始化为 nullptr
    assert(obj.ptr == nullptr);
    assert(obj.str == nullptr);

    std::cout << "  ✓ Pointers initialized to nullptr\n";
}

void test_mixed_data()
{
    std::cout << "Testing MixedData...\n";

    MixedData obj{};

    assert(obj.s == 0);
    assert(obj.l == 0L);
    assert(obj.f == 0.0f);
    assert(obj.d == 0.0);
    assert(obj.ch == 0);

    std::cout << "  ✓ Mixed data types properly initialized\n";
}

void test_bitfields()
{
    std::cout << "Testing WithBitfields...\n";

    WithBitfields obj{};

    // 位域通常会被初始化为 0，但具体行为可能依赖于编译器
    // 这些断言在大多数现代编译器上应该通过
    assert(obj.flag == 0);
    assert(obj.value == 0);
    assert(obj.regular_int == 0);

    std::cout << "  ✓ Bitfields appear to be zero-initialized\n";
}

void test_complex_nested()
{
    std::cout << "Testing ComplexNested...\n";

    ComplexNested obj{};

    // 测试所有嵌套层次
    assert(obj.simple.a == 0);
    assert(obj.simple.b == 0.0);
    assert(obj.simple.c == 0);

    for (int i = 0; i < 5; ++i)
    {
        assert(obj.arrays.arr[i] == 0);
    }
    for (int i = 0; i < 10; ++i)
    {
        assert(obj.arrays.str[i] == 0);
    }

    assert(obj.mixed.s == 0);
    assert(obj.mixed.l == 0L);
    assert(obj.mixed.f == 0.0f);
    assert(obj.mixed.d == 0.0);
    assert(obj.mixed.ch == 0);

    assert(obj.nested.x == 0);
    assert(obj.nested.y == 0.0f);
    assert(obj.nested.inner.a == 0);
    assert(obj.nested.inner.b == 0.0);
    assert(obj.nested.inner.c == 0);

    std::cout << "  ✓ Complex nested structure fully zero-initialized\n";
}

void test_partial_initialization()
{
    std::cout << "Testing partial {} initialization...\n";

    // 部分初始化 - 剩余成员应该被零初始化
    SimpleCStruct partial{42}; // 只初始化第一个成员

    assert(partial.a == 42);
    assert(partial.b == 0.0); // 应该为零
    assert(partial.c == 0);   // 应该为零

    // 嵌套结构体的部分初始化
    NestedCStruct nested_partial{100, {200}};

    assert(nested_partial.x == 100);
    assert(nested_partial.inner.a == 200);
    assert(nested_partial.inner.b == 0.0); // 应该为零
    assert(nested_partial.inner.c == 0);   // 应该为零
    assert(nested_partial.y == 0.0f);      // 应该为零

    std::cout << "  ✓ Partial initialization with zero-fill works correctly\n";

    {
        struct A
        {
            int b;
        };
        struct B
        {
            int b;
        };
        using T = std::tuple<A, B>;
        T a{{}, {}};
    }

    {
        struct A
        {
            static constexpr void function() {}
        };
        struct B : A
        {
            int b;
            double c;

            int fun()
            {
                return 0;
            }
        };
        auto b = B{.b = 1, .c = 3.0};
    }
}
// NOLINTEND