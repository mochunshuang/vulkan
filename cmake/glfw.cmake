set(GLFW_SOURCE_DIR "${CMAKE_SOURCE_DIR}/dependency/glfw-3.4")
set(GLFW_BUILD_DIR "${CMAKE_SOURCE_DIR}/depend_build/glfw-3.4")
add_subdirectory(${GLFW_SOURCE_DIR} ${GLFW_BUILD_DIR})

add_library(glfw_dep ALIAS glfw)

# rm -rf dependency/glm-1.0.2/.git 就能添加代码
# 子模块依赖
# git submodule add https://github.com/glfw/glfw.git dependency/glfw-3.4