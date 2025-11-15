
// #define _LIBCPP_MATH_H // NOTE: 禁止导入<math.h> 才不会编译错误
// #include <math.h>

import std;
import std.compat;

// #include <math.h> //NOTE: 放在这里会崩溃
// #include <cmath>

// NOTE: 有顺序要求..........

int main() // NOLINT
{
    std::vector<int> arr{1, 2, 3};
    std::println("main done: {}", arr);
    return 0;
}