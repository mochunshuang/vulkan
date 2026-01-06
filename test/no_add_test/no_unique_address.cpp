#include <iostream>

// NOLINTBEGIN
int main()
{
    static_assert(sizeof(std::allocator<void>) == 1); // 空基类优化

    struct A
    {
        int b;
    };
    static_assert(sizeof(A) == 4);
    struct B
    {
        std::allocator<void> a;
        int b;
    };
    static_assert(sizeof(B) == 2 * 4);

    struct C
    {
        [[no_unique_address]] std::allocator<void> a;
        int b;
    };
    static_assert(sizeof(C) == 1 * 4); // 空基类优化

    struct D
    {
        int b;
        [[no_unique_address]] std::allocator<void> a;
    };
    static_assert(sizeof(D) == 1 * 4); // 空基类优化 //NOTE: 无关位置

    struct E
    {
        int c;
        [[no_unique_address]] std::allocator<void> a;
        int b;
    };
    static_assert(sizeof(E) == 2 * 4); // 空基类优化

    std::cout << "main done\n";
    return 0;
}
// NOLINTEND