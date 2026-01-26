#version 450

// 128的大小就崩溃的
// 移除 scalar 布局，使用更兼容的方式
layout(push_constant, std140) uniform PushConstants {
     vec4 positions[4];
    //  vec4 colors[4];
} push;

layout(location = 0) out vec3 fragColor;

void main() {
    
    /*
    // 硬编码的顶点位置和颜色
    vec2 positions[4] = vec2[](
        vec2(-0.5, -0.5),
        vec2(0.5, -0.5),
        vec2(0.5, 0.5),
        vec2(-0.5, 0.5)
    );
    
    vec3 colors[4] = vec3[](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(1.0, 0.0, 0.0)
    );
    
    uint vertexIndex = gl_VertexIndex;
    if (vertexIndex < 4) {
        gl_Position = vec4(positions[vertexIndex], 0.0, 1.0);
        fragColor = colors[vertexIndex];
    } else {
        gl_Position = vec4(0.0);
        fragColor = vec3(0.0);
    }*/

    /*
    vec4 positions[4] = vec4[](
        vec4(-0.5, -0.5,0, 1.0f),
        vec4(0.5, -0.5,0, 1.0f),
        vec4(0.5, 0.5,0, 1.0f),
        vec4(-0.5, 0.5,0, 1.0f)
    );
    
    vec4 colors[4] = vec4[](
        vec4(1.0, 0.0, 0.0,1.),
        vec4(0.0, 1.0, 0.0,1.),
        vec4(0.0, 0.0, 1.0,1.),
        vec4(1.0, 0.0, 0.0,1.)
    );
    
    uint vertexIndex = gl_VertexIndex;
    if (vertexIndex < 4) {
        gl_Position = positions[vertexIndex];
        fragColor = colors[vertexIndex].xyz;
    } else {
        gl_Position = vec4(0.0);
        fragColor = vec3(0.0);
    }*/

//  vec4 colors[4] = vec4[](
//         vec4(1.0, 0.0, 0.0,1.),
//         vec4(0.0, 1.0, 0.0,1.),
//         vec4(0.0, 0.0, 1.0,1.),
//         vec4(1.0, 1.0, 1.0,1.)
//     );
    //  vec4 colors[3] = vec4[](
    //     vec4(1.0, 0.0, 0.0,1.),
    //     vec4(0.0, 1.0, 0.0,1.),
    //     vec4(0.0, 0.0, 1.0,1.)
    // );

        vec4 colors[6] = vec4[](
        vec4(1.0, 0.0, 0.0, 1.0),  // 红
        vec4(0.0, 1.0, 0.0, 1.0),  // 绿
        vec4(0.0, 0.0, 1.0, 1.0),  // 蓝
        vec4(1.0, 1.0, 0.0, 1.0),  // 黄
        vec4(1.0, 0.0, 1.0, 1.0),  // 紫
        vec4(0.0, 1.0, 1.0, 1.0)   // 青
    );
 gl_Position = push.positions[gl_VertexIndex];
 fragColor = colors[gl_VertexIndex].xyz;  //越界是未定义行为 
   
}