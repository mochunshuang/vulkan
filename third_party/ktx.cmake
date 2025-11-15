# git submodule add https://gh-proxy.org/https://github.com/KhronosGroup/KTX-Software.git third_party/ktx
# cd third_party/ktx
# git checkout v4.4.2
# cd ../..
# git add .
# git commit -m "Add glm submodule at tag v4.4.2"

add_subdirectory(third_party/ktx)

# 同步失败：需要手写.gitmodules 然后 git submodule update --init
# 观察文件夹，颜色一样就OK了

# NOTE: 得得同步.....。就是覆盖

# NOTE: 重置tag来消除更改：git submodule deinit -f third_party/ktx

# 删除锁
# 回到项目根目录
# cd /e/0_github_project/vulkan
# 删除锁文件
# rm -f .git/modules/third_party/ktx/index.lock
# NOTE: 锁住就关闭，重开
# 2. 直接进入ktx目录并切换到稳定版本
# cd third_party/ktx

# NOTE: "v4.2.1" 是tag
# git checkout v4.2.1

# 3. 回到主目录并提交更新
# cd ../..

# git add third_party/ktx

# git commit -m "chore: update ktx submodule to v4.2.1"