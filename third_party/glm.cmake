# NOTE: -b 是分支。以下命令是不允许的
# git submodule add -b 1.0.2 https://gh-proxy.com/https://github.com/g-truc/glm.git third_party/glm-1.0.2
# NOTE: 只能先下载，然后cheout 到指定的 tag
# git submodule add https://gh-proxy.com/https://github.com/g-truc/glm.git third_party/glm-1.0.2
# cd third_party/glm-1.0.2
# git checkout tags/1.0.2
# cd ../..
# git add .
# git commit -m "Add glm submodule at tag 1.0.2"

set(GLM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/glm-1.0.2)

if(FALSE AND MODULE_ENABLE)
    add_library(glm_modules STATIC)
    target_sources(glm_modules
        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${GLM_DIR}
        FILES
        "${GLM_DIR}/glm/glm.cppm"
    )
    target_include_directories(glm_modules
        PUBLIC
        "${GLM_DIR}"
    )
    target_compile_features(glm_modules PRIVATE cxx_std_23)

    # @see glm/glm.cppm
    target_compile_definitions(glm_modules INTERFACE GLM_GTC_INLINE_NAMESPACE GLM_EXT_INLINE_NAMESPACE GLM_GTX_INLINE_NAMESPACE)

else()
    # NOTE: #include 和 import 是不兼容的。写的.cppm不好就是G
    # NOTE: 保守一些。
    # glm
    add_library(glm INTERFACE)
    target_sources(glm INTERFACE ${GLM_DIR}/glm/glm.hpp)
    target_include_directories(glm SYSTEM INTERFACE ${GLM_DIR})

    # NOTE: .cpp内部写了 无需再配置。避免重定义警告
    # target_compile_definitions(glm INTERFACE
    # GLM_FORCE_SWIZZLE
    # GLM_FORCE_RADIANS
    # GLM_FORCE_CTOR_INIT
    # GLM_ENABLE_EXPERIMENTAL
    # )
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_definitions(glm INTERFACE GLM_FORCE_CXX14)
    endif()

    add_library(glm_modules ALIAS glm)
endif()

# NOTE: 删除
# rm -rf third_party/glm-1.0.2
# git rm --cached third_party/glm-1.0.2