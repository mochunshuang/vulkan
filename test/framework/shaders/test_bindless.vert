#version 450

#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types : require  // 添加这个扩展以支持uint64_t

// [new] 定义Vertex结构体，匹配C++端的Vertex结构
struct Vertex {
    vec2 pos;
    vec3 color;
};

// 使用标量块布局
layout(buffer_reference, scalar) buffer VertexBuffer {
    Vertex vertices[];
};

layout(buffer_reference, scalar) buffer IndexBuffer {
    uint indices[];
};

//  推送常量，包含设备地址
layout(push_constant) uniform PushConstants {
    uint64_t vertexBufferAddress;
    uint64_t indexBufferAddress;
} pc;

layout(location = 0) out vec3 fragColor;

void main() {
    // [new] 通过设备地址访问缓冲区
    IndexBuffer indexBuffer = IndexBuffer(pc.indexBufferAddress);
    VertexBuffer vertexBuffer = VertexBuffer(pc.vertexBufferAddress);
    
    // 通过gl_VertexIndex获取顶点索引
    uint vertexIndex = indexBuffer.indices[gl_VertexIndex];
    
    // 获取顶点数据
    Vertex v = vertexBuffer.vertices[vertexIndex];
    
    gl_Position = vec4(v.pos, 0.0, 1.0);
    fragColor = v.color;
}