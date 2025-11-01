#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // 包含矩阵变换函数

// NOLINTBEGIN
/*
给定一个点P=（2，1），将该点绕原点先逆时针旋转45。，再平移（1，2），
计算出变换后点的坐标（要求用齐次坐标进行计算）
*/
void test3D()
{
    // 1. 用4维齐次坐标表示点P(2, 1)，w=1（点的齐次坐标）
    glm::vec4 point(2.0f, 1.0f, 0.0f, 1.0f); // 二维点z=0，w=1

    // 2. 创建45度逆时针旋转矩阵（绕z轴）
    float angle = glm::radians(45.0f);
    glm::mat4 rotation = glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0, 0, 1));

    // 3. 执行旋转（4x4矩阵 × 4维向量，合法）
    glm::vec4 rotatedPoint = rotation * point; // 结果仍是4维向量

    // 4. 创建平移矩阵（x+1，y+2）
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 0.0f));

    // 5. 执行平移（先旋转后平移）
    glm::vec4 transformedPoint = translation * rotatedPoint;

    // 输出结果（取前两个分量，w=1可忽略）
    std::cout << "原始点: (" << point.x << ", " << point.y << ")\n";
    std::cout << "旋转后: (" << rotatedPoint.x << ", " << rotatedPoint.y << ")\n";
    std::cout << "旋转+平移后: (" << transformedPoint.x << ", " << transformedPoint.y
              << ")\n";
}

void test2D()
{
    // 1. 用2D齐次坐标表示点P(2, 1)，w=1（3维向量）
    glm::vec3 point(2.0f, 1.0f, 1.0f); // (x, y, w)，w=1表示点

    // 2. 创建45度逆时针旋转矩阵（3x3）
    float angle = glm::radians(45.0f);

    // 正确构建2D旋转矩阵
    //        [ c  -s  0 ]
    // R(θ) = [ s   c  0 ]  （列主序：矩阵按“列”存储到代码的二维数组中）
    //        [ 0   0  1 ]
    glm::mat3 rotation;
    float c = ::cos(angle);
    float s = ::sin(angle);

    // 设置旋转矩阵（列主序）
    rotation[0][0] = c; // 第一列第一行
    rotation[0][1] = s; // 第一列第二行
    rotation[0][2] = 0; // 第一列第三行

    rotation[1][0] = -s; // 第二列第一行
    rotation[1][1] = c;  // 第二列第二行
    rotation[1][2] = 0;  // 第二列第三行

    rotation[2][0] = 0; // 第三列第一行
    rotation[2][1] = 0; // 第三列第二行
    rotation[2][2] = 1; // 第三列第三行

    // 3. 执行旋转（3x3矩阵 × 3维向量）
    glm::vec3 rotatedPoint = rotation * point;

    // 4. 创建平移矩阵（3x3，平移量(1, 2)）
    //         [ 1  0  tx ]
    // T(tx) = [ 0  1  ty ]  （列主序：平移量tx/ty存储在“第2列第0/1行”）
    //         [ 0  0  1  ]
    glm::mat3 translation(1.0f); // 初始为[1,0,0; 0,1,0; 0,0,1]
    translation[2][0] = 1.0f;    // x方向平移 (第三列第一行)
    translation[2][1] = 2.0f;    // y方向平移 (第三列第二行)

    // 5. 执行平移（先旋转后平移）
    glm::vec3 transformedPoint = translation * rotation * point;

    // 输出结果（w=1，取前两个分量）
    std::cout << "2D齐次坐标计算:\n";
    std::cout << "原始点: (" << point.x << ", " << point.y << ")\n";
    std::cout << "旋转后: (" << rotatedPoint.x << ", " << rotatedPoint.y << ")\n";
    std::cout << "旋转+平移后: (" << transformedPoint.x << ", " << transformedPoint.y
              << ")\n\n";
}
int main()
{
    test3D();
    test2D();
    std::cout << "main done\n";
    return 0;
}
// NOLINTEND