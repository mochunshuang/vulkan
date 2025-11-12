import std;
import std.compat;

import math; // 直接导入模块
import foo;

int main() // NOLINT
{
    std::vector<int> arr{1, 2, 3};
    std::println("main done: {}", arr);
    auto a = 3;
    ::printf("result: %d\n", add(a, 4)); // NOLINT
    FooFunc();
    // FooFuncInternal();//NOTE: 预期错误
    return 0;
}