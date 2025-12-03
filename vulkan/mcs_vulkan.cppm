export module mcs_vulkan;

import std;
import std.compat;

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

import mcs_vulkan.api;
import mcs_vulkan.utils;

export namespace mcs::vulkan
{
    using utils::readFile;

    using api::vulkan_config;
    using api::debug_ability;
    using api::vulkan_image;
    using api::vulkan_instace;

    using api::vulkan_device;

} // namespace mcs::vulkan