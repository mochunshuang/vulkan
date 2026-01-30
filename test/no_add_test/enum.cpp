#include <iostream>

// NOLINTBEGIN
using VkFlags = int;
typedef enum VkDescriptorPoolCreateFlagBits
{
    VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT = 0x00000001,
    VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT = 0x00000002,
    VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_EXT = 0x00000004,
    VK_DESCRIPTOR_POOL_CREATE_ALLOW_OVERALLOCATION_SETS_BIT_NV = 0x00000008,
    VK_DESCRIPTOR_POOL_CREATE_ALLOW_OVERALLOCATION_POOLS_BIT_NV = 0x00000010,
    VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT =
        VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
    VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_VALVE =
        VK_DESCRIPTOR_POOL_CREATE_HOST_ONLY_BIT_EXT,
    VK_DESCRIPTOR_POOL_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VkDescriptorPoolCreateFlagBits;
typedef VkFlags VkDescriptorPoolCreateFlags;
typedef VkFlags VkDescriptorPoolResetFlags;

template <typename FlagBits>
    requires(std::is_enum_v<FlagBits>)
class VkFlagsWrapper
{
    using Underlying = std::underlying_type_t<FlagBits>;
    Underlying value;

  public:
    constexpr VkFlagsWrapper() : value(0) {}

    // 从枚举值构造
    constexpr VkFlagsWrapper(FlagBits flag) : value(static_cast<Underlying>(flag)) {}

    // 从底层值构造
    constexpr explicit VkFlagsWrapper(Underlying val) : value(val) {}

    // 复制构造
    constexpr VkFlagsWrapper(const VkFlagsWrapper &) = default;

    // 赋值运算符
    constexpr VkFlagsWrapper &operator=(FlagBits flag)
    {
        value = static_cast<Underlying>(flag);
        return *this;
    }

    constexpr VkFlagsWrapper &operator=(Underlying val)
    {
        value = val;
        return *this;
    }

    // 转换为底层类型（用于Vulkan API）
    constexpr operator Underlying() const
    {
        return value;
    }

    // 转换为包装器本身（为了重载解析）
    constexpr VkFlagsWrapper operator~() const
    {
        return VkFlagsWrapper(~value);
    }

    // 位运算
    constexpr VkFlagsWrapper operator|(FlagBits flag) const
    {
        return VkFlagsWrapper(value | static_cast<Underlying>(flag));
    }

    constexpr VkFlagsWrapper operator|(VkFlagsWrapper other) const
    {
        return VkFlagsWrapper(value | other.value);
    }

    constexpr VkFlagsWrapper operator&(FlagBits flag) const
    {
        return VkFlagsWrapper(value & static_cast<Underlying>(flag));
    }

    constexpr VkFlagsWrapper operator&(VkFlagsWrapper other) const
    {
        return VkFlagsWrapper(value & other.value);
    }

    constexpr VkFlagsWrapper operator^(FlagBits flag) const
    {
        return VkFlagsWrapper(value ^ static_cast<Underlying>(flag));
    }

    constexpr VkFlagsWrapper operator^(VkFlagsWrapper other) const
    {
        return VkFlagsWrapper(value ^ other.value);
    }

    // 复合赋值
    constexpr VkFlagsWrapper &operator|=(FlagBits flag)
    {
        value |= static_cast<Underlying>(flag);
        return *this;
    }

    constexpr VkFlagsWrapper &operator|=(VkFlagsWrapper other)
    {
        value |= other.value;
        return *this;
    }

    constexpr VkFlagsWrapper &operator&=(FlagBits flag)
    {
        value &= static_cast<Underlying>(flag);
        return *this;
    }

    constexpr VkFlagsWrapper &operator&=(VkFlagsWrapper other)
    {
        value &= other.value;
        return *this;
    }

    constexpr VkFlagsWrapper &operator^=(FlagBits flag)
    {
        value ^= static_cast<Underlying>(flag);
        return *this;
    }

    constexpr VkFlagsWrapper &operator^=(VkFlagsWrapper other)
    {
        value ^= other.value;
        return *this;
    }

    // 比较运算符
    // constexpr bool operator==(VkFlagsWrapper other) const
    // {
    //     return value == other.value;
    // }
    // constexpr bool operator!=(VkFlagsWrapper other) const
    // {
    //     return value != other.value;
    // }
    friend constexpr bool operator==(VkFlagsWrapper a,
                                     VkFlagsWrapper b) noexcept = default;

    // 测试标志
    constexpr bool test(FlagBits flag) const
    {
        return (value & static_cast<Underlying>(flag)) != 0;
    }

    // 获取底层值
    constexpr Underlying get() const
    {
        return value;
    }

    // 设置/清除标志
    constexpr void set(FlagBits flag)
    {
        value |= static_cast<Underlying>(flag);
    }
    constexpr void reset(FlagBits flag)
    {
        value &= ~static_cast<Underlying>(flag);
    }
    constexpr void reset()
    {
        value = 0;
    }

    // 切换标志
    constexpr void flip(FlagBits flag)
    {
        value ^= static_cast<Underlying>(flag);
    }
};

// NOLINTEND

int main()
{
    VkDescriptorPoolCreateFlagBits a = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    // NOTE: 下面是错误的
    //  VkDescriptorPoolCreateFlagBits b =
    //      a | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

    using T = decltype(a | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
    static_assert(std::is_same_v<T, int>);

    // a = 7; // NOTE: 错误的
    a = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    // a = 0; // NOTE: 错误的

    {
        using flag = VkFlagsWrapper<VkDescriptorPoolCreateFlagBits>;
        constexpr VkDescriptorPoolCreateFlagBits a =
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        constexpr auto v = flag(a) | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

        constexpr auto target = a | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

        static_assert(v == target);
    }
    {
        // 传统Vulkan方式
        VkDescriptorPoolCreateFlags traditionalFlags =
            VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
            VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

        // 使用包装器
        using PoolFlags = VkFlagsWrapper<VkDescriptorPoolCreateFlagBits>;

        // 方法1：构造时组合
        PoolFlags flags1 = PoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT) |
                           VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

        // 方法2：逐步添加
        PoolFlags flags2;
        flags2 |= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        flags2 |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;

        // 类型安全的操作
        if (flags1.test(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT))
        {
            std::cout << "FREE_DESCRIPTOR_SET_BIT is set\n";
        }

        flags1.reset(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
    }

    std::cout << "main done\n";
    return 0;
}