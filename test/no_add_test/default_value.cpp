#include <iostream>
#include <limits>

int main()
{
    constexpr auto UNDEFINED_DOUBLE = std::numeric_limits<double>::max();

    struct scroll_event
    {
        double xoffset{UNDEFINED_DOUBLE};
        double yoffset{UNDEFINED_DOUBLE};
    };

    constexpr auto TEST = scroll_event{.xoffset = 0};
    static_assert(TEST.xoffset == 0);
    static_assert(TEST.yoffset == UNDEFINED_DOUBLE);

    std::cout << "main done\n";
    return 0;
}