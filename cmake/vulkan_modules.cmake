set(VULKAN_SDK_DIR "E:/mysoftware/VulkanSDK/1.4.321.1" CACHE PATH "Path to Vulkan SDK installation")

if(MODULE_ENABLE)
    # NOTE:别装了 std.cppm 都是 static 的
    # ninja.exe -v 就能看到生成细节。g++ 的命令差不多
    add_library(vulkan_modules STATIC)
    target_sources(vulkan_modules
        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${VULKAN_SDK_DIR}
        FILES
        "${VULKAN_SDK_DIR}/Include/vulkan/vulkan.cppm"
    )
    target_include_directories(vulkan_modules
        PUBLIC
        "${VULKAN_SDK_DIR}/Include"
    )
    target_compile_features(vulkan_modules PRIVATE cxx_std_23)

    # config vulkan
    target_compile_definitions(vulkan_modules PUBLIC -DUSE_CPP20_MODULES -DVULKAN_HPP_NO_STRUCT_CONSTRUCTORS)

    # for <cstdlib>
    target_compile_definitions(vulkan_modules PUBLIC EXIT_SUCCESS=0 EXIT_FAILURE=1)
else()
    add_library(vulkan_dep STATIC IMPORTED)
    set_target_properties(vulkan_dep PROPERTIES
        IMPORTED_LOCATION "${VULKAN_SDK_DIR}/Lib/vulkan-1.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${VULKAN_SDK_DIR}/Include"
    )
    target_compile_definitions(vulkan_dep INTERFACE
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
    )
    add_library(vulkan_modules ALIAS vulkan_dep)
endif()
