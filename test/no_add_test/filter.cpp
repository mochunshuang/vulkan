
#include <vector>
#include <ranges>
#include <print> // C++23

// NOLINTBEGIN
struct Person
{
    std::string name;
    int age;
    bool is_active;
};

struct my_selct
{
};

int main()
{
    std::vector<Person> people = {{"Alice", 25, true},
                                  {"Bob", 17, true},
                                  {"Charlie", 30, false},
                                  {"David", 22, true},
                                  {"Eve", 16, false}};

    std::println("people begn size: {}", people.size());

    // C++23: 使用 ranges::to 直接转换
    auto adults = people |
                  std::views::filter([](const Person &p) { return p.age >= 18; }) |
                  std::views::filter([](const Person &p) { return p.is_active; }) |
                  std::ranges::to<std::vector>(); // C++23 新特性

    std::println("Adults (active):");
    for (const auto &person : adults)
    {
        std::println("  {} ({} years old)", person.name, person.age);
    }

    // 或者使用管道操作符的链式调用
    auto names = people | std::views::filter([](auto &p) { return p.age >= 18; }) |
                 std::views::transform([](auto &p) { return p.name; }) |
                 std::ranges::to<std::vector<std::string>>();
    std::println("\nAdult names: {}", names);

    std::println("people end size: {}", people.size());
}
// NOLINTEND