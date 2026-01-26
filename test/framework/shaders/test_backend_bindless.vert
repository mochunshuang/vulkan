#version 450
#extension GL_EXT_buffer_reference:require
#extension GL_EXT_scalar_block_layout:require
#extension GL_EXT_shader_explicit_arithmetic_types_int64:require

// 定义顶点数据结构（与C++端匹配）
struct VertexData{
    vec2 pos;
    vec3 color;
};

// 缓冲引用：指向VertexData数组
layout(buffer_reference,scalar)readonly buffer VertexBuffer{
    VertexData vertices[];
};
// 推送常量：包含顶点缓冲地址
layout(push_constant)uniform PushConstants{
    uint64_t vertexBufferAddress;// 顶点缓冲的设备地址
}pc;

layout(location=0)out vec3 fragColor;

void main(){
    /*
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
    }
    */
    // 用来测试推送常量是否成功
    /* if(pc.vertexBufferAddress==0){
        fragColor = vec3(1.0);
    }
    if(pc.vertexBufferAddress==1){
        fragColor = vec3(1.0,1.0,0);
    }*/
    
    VertexBuffer vertexBuffer=VertexBuffer(pc.vertexBufferAddress);
    uint vertexIndex=gl_VertexIndex;
    // 从缓冲读取顶点位置和颜色
    vec2 position=vertexBuffer.vertices[vertexIndex].pos;
    vec3 color=vertexBuffer.vertices[vertexIndex].color;
    
    gl_Position=vec4(position,0.,1.);
    fragColor=color;
}