#include <iostream>
#include <vector>
#include <utility>
#include <algorithm>

// NOLINTBEGIN
// 基类：基础资源管理器
class ResourceBase
{
  public:
    ResourceBase() = default;

    explicit ResourceBase(size_t id) noexcept : resource_id_{id}, ref_count_{new int(1)}
    {
        std::cout << "ResourceBase constructed (ID: " << resource_id_ << ")\n";
    }

    // 析构函数
    virtual ~ResourceBase() noexcept
    {
        release();
    }

    // 拷贝构造函数
    ResourceBase(const ResourceBase &other) noexcept
        : resource_id_{other.resource_id_}, ref_count_{other.ref_count_}
    {
        if (ref_count_)
            ++(*ref_count_);
        std::cout << "ResourceBase copy constructed (ID: " << resource_id_ << ")\n";
    }

    // 移动构造函数
    ResourceBase(ResourceBase &&other) noexcept
        : resource_id_{std::exchange(other.resource_id_, 0)},
          ref_count_{std::exchange(other.ref_count_, nullptr)}
    {
        std::cout << "ResourceBase move constructed (ID: " << resource_id_ << ")\n";
    }

    // 拷贝赋值运算符
    ResourceBase &operator=(const ResourceBase &other) noexcept
    {
        if (this != &other)
        {
            // 增加新资源的引用计数
            if (other.ref_count_)
            {
                ++(*other.ref_count_);
            }

            // 释放当前资源
            release();

            // 复制新资源
            resource_id_ = other.resource_id_;
            ref_count_ = other.ref_count_;

            std::cout << "ResourceBase copy assigned (ID: " << resource_id_ << ")\n";
        }
        return *this;
    }

    // 移动赋值运算符
    ResourceBase &operator=(ResourceBase &&other) noexcept
    {
        if (this != &other)
        {
            // 释放当前资源
            release();

            // 转移资源
            resource_id_ = std::exchange(other.resource_id_, 0);
            ref_count_ = std::exchange(other.ref_count_, nullptr);

            std::cout << "ResourceBase move assigned (ID: " << resource_id_ << ")\n";
        }
        return *this;
    }

    size_t id() const noexcept
    {
        return resource_id_;
    }

    int ref_count() const noexcept
    {
        return ref_count_ ? *ref_count_ : 0;
    }

  protected:
    // 释放资源的辅助函数
    void release() noexcept
    {
        if (ref_count_)
        {
            --(*ref_count_);
            if (*ref_count_ == 0)
            {
                delete ref_count_;
                std::cout << "ResourceBase destroyed (ID: " << resource_id_ << ")\n";
                resource_id_ = 0;
                ref_count_ = nullptr;
            }
            else
            {
                std::cout << "ResourceBase decremented ref (ID: " << resource_id_
                          << ", refs: " << *ref_count_ << ")\n";
            }
        }
    }

    size_t resource_id_ = 0;
    int *ref_count_ = nullptr;
};

// 派生类：带额外元数据的资源管理器
class TextureResource : public ResourceBase
{
  public:
    TextureResource() = default;

    TextureResource(size_t id, int width, int height) noexcept
        : ResourceBase(id), width_{width}, height_{height},
          pixel_data_{new float[static_cast<size_t>(width) * height]} // 防止溢出
    {
        std::cout << "TextureResource constructed " << width_ << "x" << height_ << "\n";
    }

    // 析构函数
    ~TextureResource() noexcept override
    {
        delete[] pixel_data_;
        pixel_data_ = nullptr;
        std::cout << "TextureResource destroyed (ID: " << resource_id_ << ")\n";
    }

    // 拷贝构造函数
    TextureResource(const TextureResource &other) noexcept
        : ResourceBase(other), // 调用基类拷贝构造
          width_{other.width_}, height_{other.height_}, pixel_data_{nullptr}
    {
        if (width_ > 0 && height_ > 0)
        {
            size_t size = static_cast<size_t>(width_) * height_;
            pixel_data_ = new float[size];
            std::copy(other.pixel_data_, other.pixel_data_ + size, pixel_data_);
        }
        std::cout << "TextureResource copy constructed (ID: " << resource_id_ << ")\n";
    }

    // 移动构造函数
    TextureResource(TextureResource &&other) noexcept
        : ResourceBase(std::move(other)), // 关键：移动基类部分
          width_{std::exchange(other.width_, 0)},
          height_{std::exchange(other.height_, 0)},
          pixel_data_{std::exchange(other.pixel_data_, nullptr)}
    {
        std::cout << "TextureResource move constructed (ID: " << resource_id_ << ")\n";
    }

    // 拷贝赋值运算符
    TextureResource &operator=(const TextureResource &other) noexcept
    {
        if (this != &other)
        {
            // 拷贝基类部分
            ResourceBase::operator=(other);

            // 释放当前像素数据
            delete[] pixel_data_;

            // 复制派生类部分
            width_ = other.width_;
            height_ = other.height_;
            pixel_data_ = nullptr;

            if (width_ > 0 && height_ > 0)
            {
                size_t size = static_cast<size_t>(width_) * height_;
                pixel_data_ = new float[size];
                std::copy(other.pixel_data_, other.pixel_data_ + size, pixel_data_);
            }

            std::cout << "TextureResource copy assigned (ID: " << resource_id_ << ")\n";
        }
        return *this;
    }

    // 移动赋值运算符
    TextureResource &operator=(TextureResource &&other) noexcept
    {
        if (this != &other)
        {
            // 释放当前资源
            delete[] pixel_data_;

            // 移动基类部分
            ResourceBase::operator=(std::move(other));

            // 移动派生类部分
            width_ = std::exchange(other.width_, 0);
            height_ = std::exchange(other.height_, 0);
            pixel_data_ = std::exchange(other.pixel_data_, nullptr);

            std::cout << "TextureResource move assigned (ID: " << resource_id_ << ")\n";
        }
        return *this;
    }

    void set_pixel(int x, int y, float value) noexcept
    {
        if (pixel_data_ && x >= 0 && x < width_ && y >= 0 && y < height_)
        {
            pixel_data_[y * width_ + x] = value;
        }
    }

    float get_pixel(int x, int y) const noexcept
    {
        if (pixel_data_ && x >= 0 && x < width_ && y >= 0 && y < height_)
        {
            return pixel_data_[y * width_ + x];
        }
        return 0.0f;
    }

  private:
    int width_ = 0;
    int height_ = 0;
    float *pixel_data_ = nullptr;
};

// 测试函数
void test_resource_management()
{
    std::cout << "=== Test 1: 基本构造和析构 ===\n";
    {
        TextureResource tex1(1001, 256, 256);
        tex1.set_pixel(0, 0, 1.0f);
        std::cout << "tex1 refs: " << tex1.ref_count() << "\n";
        std::cout << "Pixel (0,0): " << tex1.get_pixel(0, 0) << "\n";
    }

    std::cout << "\n=== Test 2: 拷贝构造和引用计数 ===\n";
    {
        TextureResource tex2(1002, 128, 128);
        TextureResource tex2_copy = tex2; // 拷贝构造
        std::cout << "tex2 refs: " << tex2.ref_count() << "\n";
        std::cout << "tex2_copy refs: " << tex2_copy.ref_count() << "\n";
        std::cout << "Should both be 2\n";
    }

    std::cout << "\n=== Test 3: 移动构造 ===\n";
    {
        TextureResource tex3(1003, 64, 64);
        TextureResource tex3_moved = std::move(tex3); // 移动构造
        std::cout << "tex3_moved id: " << tex3_moved.id() << "\n";
        std::cout << "tex3 id after move: " << tex3.id() << " (should be 0)\n";
        std::cout << "tex3 refs after move: " << tex3.ref_count() << " (should be 0)\n";
    }

    std::cout << "\n=== Test 4: 拷贝赋值 ===\n";
    {
        TextureResource tex4(1004, 32, 32);
        TextureResource tex4_assigned(0, 16, 16);

        std::cout << "Before assignment:\n";
        std::cout << "tex4 refs: " << tex4.ref_count() << " (should be 1)\n";
        std::cout << "tex4_assigned refs: " << tex4_assigned.ref_count()
                  << " (should be 1)\n";

        tex4_assigned = tex4; // 拷贝赋值

        std::cout << "After assignment:\n";
        std::cout << "Both should have refs=2: " << tex4.ref_count()
                  << " == " << tex4_assigned.ref_count() << "\n";
        std::cout << "tex4_assigned id: " << tex4_assigned.id() << " (should be 1004)\n";
    }

    std::cout << "\n=== Test 5: 移动赋值 ===\n";
    {
        TextureResource tex5(1005, 8, 8);
        TextureResource tex5_move_to(0, 4, 4);

        std::cout << "Before move assignment:\n";
        std::cout << "tex5 id: " << tex5.id() << "\n";
        std::cout << "tex5_move_to id: " << tex5_move_to.id() << "\n";

        tex5_move_to = std::move(tex5); // 移动赋值

        std::cout << "After move assignment:\n";
        std::cout << "tex5_move_to id: " << tex5_move_to.id() << " (should be 1005)\n";
        std::cout << "tex5 id after move: " << tex5.id() << " (should be 0)\n";
        std::cout << "tex5 refs after move: " << tex5.ref_count() << " (should be 0)\n";
    }

    std::cout << "\n=== Test 6: 自赋值 ===\n";
    {
        TextureResource tex6(1006, 10, 10);
        tex6 = tex6; // 自赋值
        std::cout << "Self-assignment completed, refs: " << tex6.ref_count()
                  << " (should be 1)\n";
    }

    std::cout << "\n=== Test 7: RAII在容器中的使用 ===\n";
    {
        std::vector<TextureResource> textures;
        textures.reserve(3);

        textures.emplace_back(2001, 10, 10);
        textures.emplace_back(2002, 20, 20);
        textures.emplace_back(2003, 30, 30);

        // 移动构造到vector
        std::vector<TextureResource> moved_textures = std::move(textures);
        std::cout << "Original vector size: " << textures.size() << " (should be 0)\n";
        std::cout << "Moved vector size: " << moved_textures.size() << " (should be 3)\n";
    }
}

int main()
{
    test_resource_management();
    std::cout << "main done";
    return 0;
}
// NOLINTEND