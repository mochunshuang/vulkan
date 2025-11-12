import std;
import std.compat;

import vulkan_hpp;

int main() // NOLINT
{

    vk::raii::Context context;
    std::println("vulkan config:");
#ifdef USE_CPP20_MODULES
    std::println("USE_CPP20_MODULES: {}", USE_CPP20_MODULES);
#endif // USE_CPP20_MODULES
#ifdef USE_CPP20_MODULES
    std::println("VULKAN_HPP_NO_STRUCT_CONSTRUCTORS: {}",
                 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS);
#endif // USE_CPP20_MODULES

    std::vector<int> arr{1, 2, 3};
    std::println("main done: {}", arr);
    return 0;
}