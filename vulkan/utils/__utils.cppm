export module mcs_vulkan.utils;

import std;
import std.compat;

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

export namespace mcs::vulkan::utils
{

    constexpr std::vector<char> readFile(const std::string &filename)
    {
        // ate：在文件末尾开始读取
        // binary：将文件作为二进制文件读取（避免文本转换）
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }
        // 在文件末尾开始读取的好处是我们可以使用读取位置来确定文件的大小并分配一个缓冲区：
        std::vector<char> buffer(file.tellg());
        // 之后，我们可以回到文件的开头并一次读取所有字节：
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        file.close();
        return buffer;
    }

} // namespace mcs::vulkan::utils
