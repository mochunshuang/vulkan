#include <iostream>
#include <unordered_map>

struct param
{
};

struct register_fun
{
    using MouseButtonCallback = void (*)(void *, param) noexcept;

    static auto &mouseButtonsCallbacks() noexcept
    {
        static std::unordered_map<void *, MouseButtonCallback> cllbacks;

        return cllbacks;
    }
    static void registerOnMouseButtonCallback(void *ptr, MouseButtonCallback fun_ptr)
    {
        mouseButtonsCallbacks().emplace(ptr, fun_ptr);
    }
    static void unregisterOnMouseButtonCallback(void *ptr)
    {
        mouseButtonsCallbacks().erase(ptr);
    }
};

struct label
{
};

struct button
{
};

int main()
{
    std::cout << "main done\n";
    return 0;
}