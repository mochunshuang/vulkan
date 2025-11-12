# NOTE: -b 是分支。以下命令是不允许的
# git submodule add -b 1.0.2 https://gh-proxy.com/https://github.com/g-truc/glm.git third_party/glm-1.0.2
# git submodule add https://gh-proxy.com/https://github.com/g-truc/glm.git third_party/glm-1.0.2
set(GLM_SOURCE_DIR "${CMAKE_SOURCE_DIR}/dependency/glm-1.0.2")
set(GLM_BUILD_DIR "${CMAKE_SOURCE_DIR}/depend_build/glm-1.0.2")

# 确保 GLM 被构建为仅头文件模式
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "Build GLM as library" FORCE)
add_subdirectory(${GLM_SOURCE_DIR} ${GLM_BUILD_DIR})
add_library(glm_dep ALIAS glm)

# NOTE: 删除
# rm -rf third_party/glm-1.0.2
# git rm --cached third_party/glm-1.0.2