# 查找 slangc 编译器
find_program(SLANG_COMPILER slangc
    PATHS "${VULKAN_SDK_DIR}/Bin"
    REQUIRED
)

if(SLANG_COMPILER)
    message(STATUS "Found Slang compiler: ${SLANG_COMPILER}")
else()
    message(FATAL_ERROR "Slang compiler not found. Please install Vulkan SDK and set VULKAN_SDK environment variable")
endif()

# 遍历目录并创建编译目标
function(auto_compile_slang_shaders NAME SHADER_INPUT_DIR SHADER_OUTPUT_DIR)
    # E:/mysoftware/VulkanSDK/1.4.321.1/Bin/slangc.exe E:\0_github_project\vulkan\test\vulkan\shaders\09_shader_base.slang -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o E:\0_github_project\vulkan\output\test_program\vulkan\shaders\09_shader_base_slang.spv
    macro(compile_slang_shader TARGET_NAME input_file out_file)
        set(SHADER_SOURCE "${SHADER_INPUT_DIR}/${input_file}")
        set(SHADER_OUTPUT "${SHADER_OUTPUT_DIR}/${out_file}")

        add_custom_target(${TARGET_NAME}
            COMMAND ${SLANG_COMPILER}
            ${SHADER_SOURCE}
            -target spirv
            -profile spirv_1_4
            -emit-spirv-directly
            -fvk-use-entrypoint-name
            -entry vertMain
            -entry fragMain
            -o ${SHADER_OUTPUT}
            DEPENDS ${SHADER_SOURCE}
            COMMENT "Compiling Slang shader: ${SHADER_SOURCE}"
            VERBATIM
        )
    endmacro()

    # SHADER_OUTPUT_DIR 目录创建
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
        RESULT_VARIABLE result
    )

    # 获取所有 .slang 文件
    file(GLOB slang_files "${SHADER_INPUT_DIR}/*.slang")

    # 创建总目标依赖列表
    set(all_targets "")

    foreach(slang_file ${slang_files})
        # 获取文件名（不含路径和扩展名）
        get_filename_component(shader_name ${slang_file} NAME_WE)

        # 设置目标名和输出文件名
        set(target_name "${shader_name}_slang")
        set(output_file "${shader_name}_slang.spv")

        # 调用现有的编译宏
        compile_slang_shader(
            ${target_name}
            "${shader_name}.slang"
            ${output_file}
        )

        # 添加到总依赖列表
        list(APPEND all_targets ${target_name})
    endforeach()

    # 创建总目标，一键编译所有着色器
    add_custom_target(${NAME} DEPENDS ${all_targets})
endfunction()
