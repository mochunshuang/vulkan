#include <cstdint>

#include <format>

namespace mcs::vulkan::core
{
    struct app_version
    {
        uint32_t major;
        uint32_t minor;
        uint32_t patch;
    };

}; // namespace mcs::vulkan::core

namespace std
{
    template <>
    struct formatter<mcs::vulkan::core::app_version>
    {
        using parse_type = mcs::vulkan::core::app_version;
        static constexpr auto parse(const std::format_parse_context &ctx) noexcept
        {
            return ctx.begin();
        }
        static constexpr auto format(const parse_type &v, std::format_context &ctx)
        {
            return std::format_to(ctx.out(), "app_version: {}.{}.{}", v.major, v.minor,
                                  v.patch);
        }
    };
}; // namespace std