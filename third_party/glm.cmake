# NOTE: -b 是分支。以下命令是不允许的
# git submodule add -b 1.0.2 https://gh-proxy.com/https://github.com/g-truc/glm.git third_party/glm-1.0.2
# NOTE: 只能先下载，然后cheout 到指定的 tag
# git submodule add https://gh-proxy.com/https://github.com/g-truc/glm.git third_party/glm-1.0.2
# cd third_party/glm-1.0.2
# git checkout tags/1.0.2
# cd ../..
# git add .
# git commit -m "Add glm submodule at tag 1.0.2"

add_library(glm INTERFACE)
set(GLM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glm-1.0.2)
target_sources(glm INTERFACE ${GLM_DIR}/glm/glm.hpp)
target_include_directories(glm SYSTEM INTERFACE ${GLM_DIR})
add_library(glm_dep ALIAS glm)

# NOTE: 删除
# rm -rf third_party/glm-1.0.2
# git rm --cached third_party/glm-1.0.2