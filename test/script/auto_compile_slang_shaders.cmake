# 查找 slangc 编译器
find_program(SLANGC_EXECUTABLE slangc
    PATHS "${VULKAN_SDK_DIR}/Bin"
    REQUIRED
)

if(SLANGC_EXECUTABLE)
    message(STATUS "Found Slang compiler: ${SLANGC_EXECUTABLE}")
else()
    message(FATAL_ERROR "Slang compiler not found. Please install Vulkan SDK and set VULKAN_SDK environment variable")
endif()

# E:/mysoftware/VulkanSDK/1.4.321.1/Bin/slangc.exe E:\0_github_project\vulkan\test\vulkan\shaders\09_shader_base.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o E:\0_github_project\vulkan\output\test_program\vulkan\shaders\09_shader_base_slang.spv
macro(add_compile_slang_shader TARGET_NAME input_file out_file entry_points)
    # NOTE: 不是字符串。不要添加双引号
    set(SLANG_SPIRV_OPTIONS
        -target spirv -profile spirv_1_4 -emit-spirv-directly
        -fvk-use-entrypoint-name
    )
    add_custom_target(${TARGET_NAME}
        COMMAND ${SLANGC_EXECUTABLE}
        ${input_file}
        ${SLANG_SPIRV_OPTIONS}
        ${entry_points}
        -o ${out_file}

        # 依赖 SHADER_OUTPUT_DIR
        DEPENDS ${SHADER_OUTPUT_DIR}
        COMMENT "Compiling Slang shader: ${SHADER_SOURCE}"
        VERBATIM
    )
endmacro()

# 遍历目录并创建编译目标
function(auto_compile_slang_shaders NAME SHADER_INPUT_DIR SHADER_OUTPUT_DIR)
    # SHADER_OUTPUT_DIR taget
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
    )

    # store all *.slang
    file(GLOB slang_files "${SHADER_INPUT_DIR}/*.slang")

    set(all_targets "")

    foreach(slang_file ${slang_files})
        # 获取文件名（不含路径和扩展名）
        get_filename_component(shader_name ${slang_file} NAME_WE)

        set(target_name "${shader_name}_spv")
        set(output_file "${shader_name}.spv")

        # 实例化宏
        set(ENTRY_POINTS -entry vertMain -entry fragMain)
        add_compile_slang_shader(
            ${target_name}
            "${SHADER_INPUT_DIR}/${shader_name}.slang"
            "${SHADER_OUTPUT_DIR}/${output_file}"
            "${ENTRY_POINTS}" # 作为引用的字符串传递
        )
        list(APPEND all_targets ${target_name})
    endforeach()

    add_custom_target(${NAME} DEPENDS ${all_targets})
endfunction()
