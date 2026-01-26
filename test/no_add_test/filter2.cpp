#include <iostream>
#include <vector>
#include <ranges>
#include <algorithm>
#include <print>

// NOLINTBEGIN
// 示例数据
struct Device
{
    std::string name;
    int score;
    bool supported;
};

int main()
{
    // 原始数据
    std::vector<Device> devices = {{"Device A", 85, true},
                                   {"Device B", 92, true},
                                   {"Device C", 45, false},
                                   {"Device D", 78, true},
                                   {"Device E", 95, true}};

    // ========== 方案1：直接使用 views::filter ==========

    // 方法1：获取满足条件的索引
    auto high_score_indices =
        std::views::iota(0u, static_cast<uint32_t>(devices.size())) |
        std::views::filter([&devices](uint32_t idx) { return devices[idx].score > 80; });

    std::println("High score devices (score > 80):");
    for (uint32_t idx : high_score_indices)
    {
        std::println("  Index {}: {} (score: {})", idx, devices[idx].name,
                     devices[idx].score);
    }

    // 方法2：链式过滤
    auto good_devices_indices = std::views::iota(0u, devices.size()) |
                                std::views::filter([&devices](size_t idx) {
                                    return devices[idx].score > 80; // 分数 > 80
                                }) |
                                std::views::filter([&devices](size_t idx) {
                                    return devices[idx].supported; // 且支持的
                                });

    std::println("\nGood and supported devices:");
    for (size_t idx : good_devices_indices)
    {
        std::println("  Index {}: {}", idx, devices[idx].name);
    }

    // ========== 方案2：创建便捷的辅助函数 ==========

    // 辅助函数：创建索引视图
    auto create_index_view = [](const auto &container) {
        return std::views::iota(0u, static_cast<uint32_t>(container.size()));
    };

    // 辅助函数：过滤并返回索引
    auto filter_indices = [&](auto predicate) {
        return create_index_view(devices) |
               std::views::filter([&devices, predicate](uint32_t idx) {
                   return predicate(devices[idx]);
               });
    };

    auto supported_indices = filter_indices([](const Device &d) { return d.supported; });

    std::println("\nSupported devices indices:");
    for (uint32_t idx : supported_indices)
    {
        std::println("  Index {}: {}", idx, devices[idx].name);
    }

    // ========== 方案3：一步获取结果 ==========

    // 获取第一个满足条件的索引
    auto find_first = [&devices](auto predicate) -> std::optional<size_t> {
        auto view = std::views::iota(0u, devices.size()) |
                    std::views::filter([&devices, predicate](size_t idx) {
                        return predicate(devices[idx]);
                    });

        auto it = std::ranges::begin(view);
        if (it != std::ranges::end(view))
        {
            return *it;
        }
        return std::nullopt;
    };

    auto best_idx =
        find_first([](const Device &d) { return d.score > 90 && d.supported; });

    if (best_idx)
    {
        std::println("\nBest device found at index {}: {}", *best_idx,
                     devices[*best_idx].name);
    }

    // ========== 方案4：使用 ranges::to 获取 vector ==========

    // C++23: 直接转换为 vector
    auto high_score_idx_vec =
        std::views::iota(0u, devices.size()) |
        std::views::filter([&devices](size_t idx) { return devices[idx].score > 70; }) |
        std::ranges::to<std::vector<size_t>>(); // C++23 特性！

    std::println("\nHigh score indices (as vector):");
    for (size_t idx : high_score_idx_vec)
    {
        std::println("  {}", idx);
    }

    return 0;
}
// NOLINTEND