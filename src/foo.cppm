
module;

#include <iostream>

export module foo;

void FooFuncInternal() // NOLINT
{
    [[maybe_unused]] auto a = 0;
    std::cout << "Foo!" << '\n';
}

export void FooFunc()
{
    FooFuncInternal();
}