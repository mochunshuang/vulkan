# git submodule add https://gh-proxy.org/https://github.com/KhronosGroup/KTX-Software.git third_party/ktx
# cd third_party/ktx
# git checkout tags/v4.4.2
# cd ../..
# git add .
# git commit -m "Add glm submodule at tag v4.4.2"

add_library(ktx INTERFACE)
target_include_directories(stb SYSTEM INTERFACE "${CMAKE_SOURCE_DIR}/third_party/ktx")

# 同步失败：需要手写.gitmodules 然后 git submodule update --init
# 观察文件夹，颜色一样就OK了