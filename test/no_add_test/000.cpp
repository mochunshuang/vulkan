#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <cstring>   // 添加memcpy
#include <algorithm> // 添加find

namespace fs = std::filesystem;

class SimpleM3U8Replacer
{
  public:
    static constexpr const char *OLD_PATH = "file:///sdcard/Download/UCDownloads";
    static constexpr const char *NEW_PATH = "file:///sdcard/Download/UCDownloads2";

    // 修正：实际长度应该是37和38，不是45和46
    static constexpr size_t OLD_LEN = 37; // strlen(OLD_PATH)
    static constexpr size_t NEW_LEN = 38; // strlen(NEW_PATH)

  public:
    // 改进的二进制查找和替换
    static bool binaryReplaceInFile(const fs::path &filepath)
    {
        std::cout << "\n正在处理文件: " << filepath << std::endl;

        // 方法1：使用二进制方式读取整个文件到字符串
        std::ifstream in(filepath, std::ios::binary);
        if (!in)
        {
            std::cerr << "无法打开文件: " << filepath << std::endl;
            return false;
        }

        // 读取整个文件到字符串
        std::string content((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
        in.close();

        std::cout << "文件大小: " << content.size() << " 字节" << std::endl;

        // 查找要替换的字符串
        size_t pos = 0;
        int replace_count = 0;
        bool replaced = false;

        while ((pos = content.find(OLD_PATH, pos)) != std::string::npos)
        {
            std::cout << "在位置 " << pos << " 找到匹配项!" << std::endl;

            // 显示匹配位置的上下文
            size_t start = (pos > 50) ? pos - 50 : 0;
            size_t length = std::min(OLD_LEN + 20, content.size() - start);
            std::cout << "上下文: ..." << content.substr(start, length) << "..."
                      << std::endl;

            // 进行替换
            content.replace(pos, OLD_LEN, NEW_PATH);
            pos += NEW_LEN; // 跳过新插入的部分，继续查找
            replace_count++;
            replaced = true;
        }

        std::cout << "共找到并替换 " << replace_count << " 处" << std::endl;

        if (!replaced)
        {
            std::cout << "未找到需要替换的内容" << std::endl;

            // 在文件中搜索部分匹配，看看是不是路径有变化
            std::cout << "尝试搜索部分匹配..." << std::endl;
            size_t search_pos = 0;
            int partial_matches = 0;
            while ((search_pos = content.find("file://", search_pos)) !=
                   std::string::npos)
            {
                size_t end_pos = std::min(search_pos + 100, content.size());
                std::cout << "在位置 " << search_pos
                          << " 找到file://: " << content.substr(search_pos, 50)
                          << std::endl;
                search_pos++;
                partial_matches++;
                if (partial_matches > 5)
                    break; // 只显示前几个
            }
            return true;
        }

        // 写回文件
        std::ofstream out(filepath, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            std::cerr << "无法写入文件: " << filepath << std::endl;
            return false;
        }

        out.write(content.c_str(), content.size());
        bool success = out.good();
        out.close();

        if (success)
        {
            std::cout << "写入成功，新文件大小: " << content.size() << " 字节"
                      << std::endl;
        }
        else
        {
            std::cerr << "写入失败!" << std::endl;
        }

        return success;
    }

    // 简化验证函数
    static bool simpleVerify(const fs::path &file_a, const fs::path &file_b)
    {
        // 读取两个文件
        std::ifstream a(file_a, std::ios::binary);
        std::ifstream b(file_b, std::ios::binary);

        if (!a || !b)
        {
            std::cout << "无法打开文件进行验证" << std::endl;
            return false;
        }

        std::string content_a((std::istreambuf_iterator<char>(a)),
                              std::istreambuf_iterator<char>());
        std::string content_b((std::istreambuf_iterator<char>(b)),
                              std::istreambuf_iterator<char>());

        // 简单的验证：统计OLD_PATH和NEW_PATH的出现次数
        size_t count_a_new = 0;
        size_t count_b_old = 0;
        size_t pos = 0;

        while ((pos = content_a.find(NEW_PATH, pos)) != std::string::npos)
        {
            count_a_new++;
            pos += NEW_LEN;
        }

        pos = 0;
        while ((pos = content_b.find(OLD_PATH, pos)) != std::string::npos)
        {
            count_b_old++;
            pos += OLD_LEN;
        }

        std::cout << "文件A中NEW_PATH出现次数: " << count_a_new
                  << ", 文件B中OLD_PATH出现次数: " << count_b_old << std::endl;

        return count_a_new == count_b_old;
    }

    // 处理A目录
    static void processDirectoryA(const fs::path &dir_a)
    {
        std::cout << "处理目录A: " << dir_a << std::endl;

        int success = 0, fail = 0;
        std::vector<std::string> failed_files;

        for (const auto &entry : fs::directory_iterator(dir_a))
        {
            try
            {
                if (entry.is_regular_file() && entry.path().extension() == ".m3u8")
                {
                    std::string filename = entry.path().filename().string();
                    std::cout << "\n处理: " << filename << " ... ";

                    if (binaryReplaceInFile(entry.path()))
                    {
                        std::cout << "替换完成" << std::endl;
                        ++success;
                    }
                    else
                    {
                        std::cout << "失败" << std::endl;
                        ++fail;
                        failed_files.push_back(filename);
                    }
                }
            }
            catch (const std::exception &e)
            {
                std::cout << "异常: " << e.what() << std::endl;
                ++fail;
                failed_files.push_back(entry.path().filename().string());
            }
        }

        std::cout << "\n\nA目录处理结果: 成功 " << success << " 个, 失败 " << fail
                  << " 个" << std::endl;
        if (!failed_files.empty())
        {
            std::cout << "失败文件:\n";
            for (const auto &f : failed_files)
            {
                std::cout << "  " << f << std::endl;
            }
        }
    }

    // 用B目录校验
    static void verifyWithDirectoryB(const fs::path &dir_a, const fs::path &dir_b)
    {
        std::cout << "\n\n用B目录校验: " << dir_b << std::endl;

        // 收集A目录文件
        std::map<std::string, fs::path> files_a;
        for (const auto &entry : fs::directory_iterator(dir_a))
        {
            try
            {
                if (entry.is_regular_file() && entry.path().extension() == ".m3u8")
                {
                    files_a[entry.path().filename().string()] = entry.path();
                }
            }
            catch (...)
            {
            }
        }

        int success = 0, fail = 0;
        std::vector<std::string> failed_files;

        // 逐个校验
        for (const auto &entry : fs::directory_iterator(dir_b))
        {
            try
            {
                if (entry.is_regular_file() && entry.path().extension() == ".m3u8")
                {
                    std::string filename = entry.path().filename().string();

                    auto it = files_a.find(filename);
                    if (it != files_a.end())
                    {
                        std::cout << "\n校验: " << filename << " ... ";

                        if (simpleVerify(it->second, entry.path()))
                        {
                            std::cout << "通过" << std::endl;
                            ++success;
                        }
                        else
                        {
                            std::cout << "失败" << std::endl;
                            ++fail;
                            failed_files.push_back(filename);
                        }

                        files_a.erase(it);
                    }
                }
            }
            catch (...)
            {
                std::cout << "跳过" << std::endl;
            }
        }

        // A目录有但B目录没有的文件
        for (const auto &[filename, _] : files_a)
        {
            std::cout << "警告: " << filename << " 在B目录中不存在" << std::endl;
            ++fail;
            failed_files.push_back(filename + " (B目录缺失)");
        }

        std::cout << "\n校验结果: 通过 " << success << " 个, 失败 " << fail << " 个"
                  << std::endl;
        if (!failed_files.empty())
        {
            std::cout << "失败文件:\n";
            for (const auto &f : failed_files)
            {
                std::cout << "  " << f << std::endl;
            }
        }
    }

    // 简单的测试函数，用于调试
    static void testFindAndReplace()
    {
        std::cout << "\n=== 测试查找和替换 ===" << std::endl;
        std::cout << "OLD_PATH: " << OLD_PATH << " (长度: " << OLD_LEN << ")"
                  << std::endl;
        std::cout << "NEW_PATH: " << NEW_PATH << " (长度: " << NEW_LEN << ")"
                  << std::endl;

        // 测试字符串
        std::string test = "file:///sdcard/Download/UCDownloads/VideoData/test.ts";
        size_t pos = test.find(OLD_PATH);
        if (pos != std::string::npos)
        {
            std::cout << "测试成功! 在位置 " << pos << " 找到匹配" << std::endl;
        }
        else
        {
            std::cout << "测试失败! 没有找到匹配" << std::endl;
        }
    }
};

int main()
{
    const fs::path dir_a = "C:/Users/mcs/Desktop/11/a";
    const fs::path dir_b = "C:/Users/mcs/Desktop/11/b";

    std::cout << "M3U8文件路径替换工具" << std::endl;
    std::cout << "替换: " << SimpleM3U8Replacer::OLD_PATH << " -> "
              << SimpleM3U8Replacer::NEW_PATH << std::endl;
    std::cout << "路径长度: OLD_LEN=" << SimpleM3U8Replacer::OLD_LEN
              << ", NEW_LEN=" << SimpleM3U8Replacer::NEW_LEN << std::endl;
    std::cout << "====================================" << std::endl;

    // 先运行测试
    SimpleM3U8Replacer::testFindAndReplace();

    std::cout << "\n====================================" << std::endl;

    // 1. 替换A目录
    SimpleM3U8Replacer::processDirectoryA(dir_a);

    std::cout << "\n====================================" << std::endl;

    // 2. 用B目录校验
    SimpleM3U8Replacer::verifyWithDirectoryB(dir_a, dir_b);

    return 0;
}