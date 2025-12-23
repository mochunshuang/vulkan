#pragma once

#include "./utils/mcslog.hpp"

#include "./vk_api/vk_debug_api.hpp"
#include "./sType.hpp"

namespace mcs::vulkan
{
    struct debug_extension : vk_api::vk_debug_api
    {
        /// @brief A debug callback called from Vulkan validation layers.
        static VKAPI_ATTR VkBool32 VKAPI_CALL
        defaultDebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                             VkDebugUtilsMessageTypeFlagsEXT /*message_types*/,
                             const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                             void * /*user_data*/)
        {

            if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
            {
                MCSLOG_ERROR("{} Validation Layer: Error: {}: {}",
                             callback_data->messageIdNumber,
                             callback_data->pMessageIdName, callback_data->pMessage);
            }
            else if ((message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
            {
                MCSLOG_WARN("{} Validation Layer: Warning: {}: {}",
                            callback_data->messageIdNumber, callback_data->pMessageIdName,
                            callback_data->pMessage);
            }
            else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) !=
                     0)
            {
                MCSLOG_INFO("{} Validation Layer: Information: {}: {}",
                            callback_data->messageIdNumber, callback_data->pMessageIdName,
                            callback_data->pMessage);
            }
            else if ((message_severity &
                      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) != 0)
            {
                MCSLOG_DEBUG("{} Validation Layer: Verbose: {}: {}",
                             callback_data->messageIdNumber,
                             callback_data->pMessageIdName, callback_data->pMessage);
            }
            return VK_FALSE;
        }

        static constexpr VkDebugUtilsMessengerCreateInfoEXT defaultCreateInfo() noexcept
        {
            return {.sType = sType<VkDebugUtilsMessengerCreateInfoEXT>(),
                    .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                       VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                    .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                    .pfnUserCallback = &defaultDebugCallback};
        }
    };

}; // namespace mcs::vulkan