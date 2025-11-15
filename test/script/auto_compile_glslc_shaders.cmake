
find_program(GLSLC_EXECUTABLE glslc
    PATHS "${VULKAN_SDK_DIR}/Bin"
    REQUIRED
)

if(GLSLC_EXECUTABLE)
    message(STATUS "Found glslc compiler: ${GLSLC_EXECUTABLE}")
else()
    message(FATAL_ERROR "glslc compiler not found. Please install Vulkan SDK and set VULKAN_SDK environment variable")
endif()

# E:/mysoftware/VulkanSDK/1.4.321.1/Bin/glslc.exe 27_shader_textures.vert -o 27_vert.spv
# E:/mysoftware/VulkanSDK/1.4.321.1/Bin/glslc.exe 27_shader_textures.frag -o 27_frag.spv
macro(add_compile_glslc_shader TARGET_NAME input_file out_file)
    add_custom_target(${TARGET_NAME}
        COMMAND ${GLSLC_EXECUTABLE}
        ${input_file}
        -o ${out_file}
        DEPENDS ${SHADER_OUTPUT_DIR}
        COMMENT "Compiling glslc shader: ${shader_name}"
        VERBATIM
    )
endmacro()

function(auto_compile_vert_shaders NAME SHADER_INPUT_DIR SHADER_OUTPUT_DIR)
    # SHADER_OUTPUT_DIR taget
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
    )

    # store all *.vert
    file(GLOB files "${SHADER_INPUT_DIR}/*.vert")

    set(all_targets "")

    foreach(cur_file ${files})
        # 获取文件名（不含路径和扩展名）
        get_filename_component(shader_name ${cur_file} NAME_WE)

        set(target_name "${shader_name}_vert")
        set(output_file "${shader_name}_vert.spv")

        add_compile_glslc_shader(
            ${target_name}
            "${cur_file}"
            "${SHADER_OUTPUT_DIR}/${output_file}"
        )
        list(APPEND all_targets ${target_name})
    endforeach()

    add_custom_target(${NAME} DEPENDS ${all_targets})
endfunction()

function(auto_compile_frag_shaders NAME SHADER_INPUT_DIR SHADER_OUTPUT_DIR)
    # SHADER_OUTPUT_DIR taget
    add_custom_command(
        OUTPUT ${SHADER_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUTPUT_DIR}
    )

    # store all *.frag
    file(GLOB files "${SHADER_INPUT_DIR}/*.frag")

    set(all_targets "")

    foreach(cur_file ${files})
        get_filename_component(shader_name ${cur_file} NAME_WE)

        set(target_name "${shader_name}_frag")
        set(output_file "${shader_name}_frag.spv")

        add_compile_glslc_shader(
            ${target_name}
            "${cur_file}"
            "${SHADER_OUTPUT_DIR}/${output_file}"
        )
        list(APPEND all_targets ${target_name})
    endforeach()

    add_custom_target(${NAME} DEPENDS ${all_targets})
endfunction()
